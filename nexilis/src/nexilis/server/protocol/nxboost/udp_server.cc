/* Copyright (C) 2026 Valtteri Viirret
   This file is part of the Nexilis Project.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this file.  If not, see <https://gnu.org>. */

#include <nexilis/logger/log.hh>
#include <nexilis/ports.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/message/auth_message.hh>
#include <nexilis/server/protocol/nxboost/udp_server.hh>
#include <nexilis/udp_auth.hh>

#include <openssl/rand.h>

#include <boost/asio/ip/address.hpp>
#include <boost/system/system_error.hpp>

namespace nexilis::server::nxboost
{

UDPServer::UDPServer(const ServerConfig& settings)
    : ServerProtocol(settings),
      NxClass("server::nxboost::UDPServer"),
      m_stopped(std::make_unique<std::atomic<bool>>(false)),
      m_ioContext(std::make_unique<boost::asio::io_context>()),
      m_mutex(std::make_unique<std::mutex>()),
      m_remoteEndpoint(boost::asio::ip::udp::v4(), Ports::getBoostUDPPort()),
      m_socket(*m_ioContext, m_remoteEndpoint),
      m_receiveBuffer(NEXILIS_BUFFER)
{
    // Set socket option to allow address reuse
    boost::asio::ip::udp::socket::reuse_address reuse(true);
    m_socket.set_option(reuse);
    if (RAND_bytes(m_cookieKey.data(), m_cookieKey.size()) != 1)
        throw std::runtime_error("Failed to create UDP cookie key");
}

UDPServer::~UDPServer()
{
    stop();
    Util::cleanupPortFile(getType());
}

UDPServer::UDPServer(UDPServer&& other)
    : NXProtocol(std::move(other)),
      ServerProtocol(std::move(other)),
      NxClass(std::move(other)),
      m_stopped(std::move(other.m_stopped) ? std::move(other.m_stopped) : std::make_unique<std::atomic<bool>>(false)),
      m_ioContext(std::move(other.m_ioContext)),
      m_mutex(std::move(other.m_mutex)),
      m_remoteEndpoint(std::move(other.m_remoteEndpoint)),
      m_socket(std::move(other.m_socket)),
      m_receiveBuffer(std::move(other.m_receiveBuffer)),
      m_ioContextThread(std::move(other.m_ioContextThread)),
      m_receiveThread(std::move(other.m_receiveThread))
{
    m_cookieKey = other.m_cookieKey;
    m_clientEndpoints = std::move(other.m_clientEndpoints);
}

UDPServer& UDPServer::operator=(UDPServer&& other)
{
    if (this != &other)
    {
        static_cast<NXProtocol&>(*this) = static_cast<NXProtocol&&>(other);
        static_cast<ServerProtocol&>(*this) = static_cast<ServerProtocol&&>(other);
        static_cast<NxClass&>(*this) = static_cast<NxClass&&>(other);

        m_stopped = std::move(other.m_stopped);
        if (!m_stopped)
        {
            m_stopped = std::make_unique<std::atomic<bool>>(false);
        }
        m_ioContext = std::move(other.m_ioContext);
        m_mutex = std::move(other.m_mutex);
        m_remoteEndpoint = std::move(other.m_remoteEndpoint);
        m_socket = std::move(other.m_socket);
        m_receiveBuffer = std::move(other.m_receiveBuffer);
        m_ioContextThread = std::move(other.m_ioContextThread);
        m_receiveThread = std::move(other.m_receiveThread);
        m_cookieKey = other.m_cookieKey;
        m_clientEndpoints = std::move(other.m_clientEndpoints);
    }
    return *this;
}

void UDPServer::start()
{
    auto local_endpoint = m_socket.local_endpoint();
    m_serverPort = local_endpoint.port();
    Log::debug("Boost UDP server started on port: ", m_serverPort);
    if (!Util::writePortToFile(m_serverPort, getType()))
    {
        Log::error("Failed to write Boost UDP server port to a file");
    }

    m_ioContextThread = std::thread([this]()
                                    { m_ioContext->run(); });

    m_receiveThread = std::thread(&UDPServer::receiveFromClients, this);
}

void UDPServer::stop()
{
    if (!m_stopped || m_stopped->exchange(true))
    {
        Log::debug("BoostUDPServer stop already in progress or completed.");
        return;
    }

    boost::system::error_code ec;

    if (m_socket.cancel(ec))
    {
        Log::error("Error cancelling socket: ", ec.message());
    }
    if (ec)
    {
        Log::error("Cancelling error: ", ec.message());
    }

    if (m_ioContext)
    {
        Log::debug("BoostUDPServer stopping io_context");
        m_ioContext->stop();
    }

    if (m_socket.is_open())
    {
        m_socket.shutdown(boost::asio::ip::udp::socket::shutdown_both, ec);
        if (m_socket.close(ec))
        {
            Log::error("Error closing socket: ", ec.message());
        }
        if (ec)
        {
            Log::error("Socket closing error: ", ec.message());
        }
    }

    if (m_ioContextThread.joinable())
    {
        Log::debug("BoostUDPServer closing ioContextThread");
        m_ioContextThread.join();
    }

    if (m_receiveThread.joinable())
    {
        Log::debug("BoostUDPServer closing receiveThread");
        m_receiveThread.join();
    }
    Log::debug("BoostUDPServer stopped");
}

void UDPServer::receiveFromClients()
{
    while (m_socket.is_open())
    {
        try
        {
            m_receiveBuffer.resize(NEXILIS_BUFFER);

            size_t bytes_received = m_socket.receive_from(boost::asio::buffer(m_receiveBuffer), m_remoteEndpoint);

            if (m_stopped->load())
            {
                break;
            }

            if (bytes_received == 0)
            {
                continue;
            }

            std::string address = m_remoteEndpoint.address().to_string();
            // Create a new buffer containing only the received bytes.
            nx_data received_message(m_receiveBuffer.begin(), m_receiveBuffer.begin() + bytes_received);

            const auto senderEndpoint = m_remoteEndpoint;
            const auto endpointKey = address + ":" + std::to_string(senderEndpoint.port());
            nx_data payload;
            if (udp_auth::hasPrefix(received_message, udp_auth::hello))
            {
                if (received_message.size() == udp_auth::makeHello().size())
                {
                    auto challenge = udp_auth::makeChallenge(m_cookieKey, endpointKey);
                    if (!challenge.empty())
                        m_socket.send_to(boost::asio::buffer(challenge), senderEndpoint);
                }
                continue;
            }
            if (udp_auth::hasPrefix(received_message, udp_auth::authentication))
            {
                if (!udp_auth::checkAuthentication(received_message, m_cookieKey, endpointKey, payload))
                    continue;
                if (payload.size() >= 8 && ClientStorage::getClientById(Util::uint64FromFront(payload)))
                    continue;
            }
            else if (udp_auth::hasPrefix(received_message, udp_auth::session))
            {
                if (received_message.size() < 4 + 8)
                    continue;
                nx_data idBytes(received_message.begin() + 4, received_message.begin() + 12);
                auto* user = ClientStorage::getClientById(Util::uint64FromFront(idBytes));
                if (!user || !udp_auth::readSession(received_message, user->getUdpToken(), payload))
                    continue;
                auto bound = m_clientEndpoints.find(user->getId());
                if (bound != m_clientEndpoints.end() && bound->second != senderEndpoint)
                    continue;
            }
            else
            {
                continue;
            }

            auto handledMessage = getMessageHandler().readMessage(address, payload, &getSettings());

            if (!handledMessage || !handledMessage->getUser())
            {
                Log::error(header(), "Received invalid message or null user");
                continue;
            }

            if (!handledMessage->getUser()->isBoostUDPSet())
            {
                m_clientEndpoints[handledMessage->getUser()->getId()] = senderEndpoint;
                // clang-format off
                handledMessage->getUser()->setBoostUDPSend([this, senderEndpoint](const nx_data& bytes)
                {
                    if (m_socket.send_to(boost::asio::buffer(bytes), senderEndpoint) == 0)
                    {
                        Log::error("Failed to send message to client");
                    }
                });
                // clang-format on
            }

            auto msg_type = handledMessage->getType();

            if (msg_type == BaseMessage::Type::auth_message)
            {
                auto auth_ptr = static_cast<AuthMessage*>(handledMessage.get());
                CommandResult passCommand = getCommand().read(
                        auth_ptr->getData()[0], *auth_ptr->getUser(), *this, auth_ptr->getMessageId());
                if (passCommand != CommandResult::success)
                {
                    Log::error(header(), "UDP auth failed");
                }
            }
            else if (msg_type == BaseMessage::Type::message)
            {
                auto msg_ptr = static_cast<Message*>(handledMessage.get());

                CommandResult passCommand = getCommand().read(msg_ptr->getData(), *msg_ptr->getUser(), *this, msg_ptr->getMessageId());
                switch (passCommand)
                {
                    case CommandResult::success:
                        Log::debug("UDPServer: Passed");
                        break;

                    default:
                        Log::error("UDPServer: Command failed with result: ", asString(passCommand));
                }
            }
            else
            {
                Log::critical(header(), "Unrecognized message type");
            }
        }
        catch (const boost::system::system_error& e)
        {
            Log::error("Error receiving message: ", e.what());
        }
    }

    Log::error("Socket is not open!");
}

} // namespace nexilis::server::nxboost
