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
#include <nexilis/server/command/command.hh>
#include <nexilis/server/message/auth_message.hh>
#include <nexilis/server/protocol/nxboost/udp_server.hh>

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
}

UDPServer::~UDPServer()
{
    stop();
    Util::cleanupPortFile(getType());
}

UDPServer::UDPServer(UDPServer&& other)
    : Protocol(std::move(other)),
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
}

UDPServer& UDPServer::operator=(UDPServer&& other)
{
    if (this != &other)
    {
        static_cast<Protocol&>(*this) = static_cast<Protocol&&>(other);
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
            Log::info("Received from ", address, bytes_received, " bytes.", " Data: ", m_receiveBuffer.data());

            // Create a new buffer containing only the received bytes.
            nx_data received_message(m_receiveBuffer.begin(), m_receiveBuffer.begin() + bytes_received);

            auto handledMessage = getMessageHandler().readMessage(address, received_message, &getSettings());

            if (!handledMessage || !handledMessage->getUser())
            {
                Log::error(header(), "Received invalid message or null user");
                continue;
            }

            if (!handledMessage->getUser()->isBoostUDPSet())
            {
                auto senderEndpoint = m_remoteEndpoint;
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
                        Log::info("UDPServer: Passed");
                        break;

                    default:
                        Log::error("UDPServer: Something FAILED");
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
