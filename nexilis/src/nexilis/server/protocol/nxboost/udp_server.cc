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
#include <chrono>
#include <stdexcept>

namespace nexilis::server::nxboost
{

struct UDPServer::SocketState
{
    SocketState()
        : socket(context, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), Ports::getBoostUDPPort()))
    {
        socket.set_option(boost::asio::ip::udp::socket::reuse_address(true));
        socket.non_blocking(true);
    }

    void disableSends()
    {
        std::lock_guard lock(mutex);
        acceptingSends = false;
    }

    void close()
    {
        std::lock_guard lock(mutex);
        acceptingSends = false;
        boost::system::error_code ec;
        socket.close(ec);
    }

    void send(const boost::asio::ip::udp::endpoint& endpoint, const nx_data& bytes)
    {
        std::lock_guard lock(mutex);
        if (!acceptingSends || !socket.is_open())
            throw std::runtime_error("Boost UDP socket is closed");
        socket.send_to(boost::asio::buffer(bytes), endpoint);
    }

    size_t receive(nx_data& bytes, boost::asio::ip::udp::endpoint& endpoint,
                   boost::system::error_code& ec)
    {
        std::lock_guard lock(mutex);
        return socket.receive_from(boost::asio::buffer(bytes), endpoint, 0, ec);
    }

    // All operations on this Asio socket share the mutex. Nonblocking reads
    // release it immediately so sends and shutdown cannot wait on a datagram.
    boost::asio::io_context context;
    boost::asio::ip::udp::socket socket;
    std::mutex mutex;
    bool acceptingSends = true;
};

UDPServer& UDPServer::movable(UDPServer& protocol)
{
    if (protocol.m_receiveThread.joinable())
        throw std::logic_error("Stop a Boost UDP server before moving it");
    return protocol;
}

UDPServer::UDPServer(const ServerConfig& settings)
    : ServerProtocol(settings),
      NxClass("server::nxboost::UDPServer"),
      m_stopped(std::make_unique<std::atomic<bool>>(false)),
      m_socketState(std::make_shared<SocketState>())
{
    m_serverPort = m_socketState->socket.local_endpoint().port();
    if (RAND_bytes(m_cookieKey.data(), m_cookieKey.size()) != 1)
        throw std::runtime_error("Failed to create UDP cookie key");
}

UDPServer::~UDPServer()
{
    stop();
    Util::cleanupPortFile(getType());
}

UDPServer::UDPServer(UDPServer&& other)
    : NXProtocol(std::move(movable(other))),
      ServerProtocol(std::move(other)),
      NxClass(std::move(other)),
      m_stopped(std::move(other.m_stopped)),
      m_socketState(std::move(other.m_socketState)),
      m_receiveThread(std::move(other.m_receiveThread)),
      m_serverPort(other.m_serverPort),
      m_cookieKey(other.m_cookieKey),
      m_clientEndpoints(std::move(other.m_clientEndpoints))
{
}

UDPServer& UDPServer::operator=(UDPServer&& other)
{
    if (this != &other)
    {
        movable(other);
        stop();
        static_cast<NXProtocol&>(*this) = static_cast<NXProtocol&&>(other);
        static_cast<ServerProtocol&>(*this) = static_cast<ServerProtocol&&>(other);
        static_cast<NxClass&>(*this) = static_cast<NxClass&&>(other);
        m_stopped = std::move(other.m_stopped);
        m_socketState = std::move(other.m_socketState);
        m_receiveThread = std::move(other.m_receiveThread);
        m_serverPort = other.m_serverPort;
        m_cookieKey = other.m_cookieKey;
        m_clientEndpoints = std::move(other.m_clientEndpoints);
    }
    return *this;
}

void UDPServer::start()
{
    if (!m_stopped || m_stopped->load() || m_receiveThread.joinable())
        return;

    Log::debug("Boost UDP server started on port: ", m_serverPort);
    if (!Util::writePortToFile(m_serverPort, getType()))
        Log::error("Failed to write Boost UDP server port to a file");

    m_receiveThread = std::thread(&UDPServer::receiveFromClients, this);
}

void UDPServer::stop()
{
    if (!m_stopped)
        return;
    m_stopped->store(true);
    m_socketState->disableSends();
    // An application callback may stop dispatch on the receive worker. An
    // external stop/destructor must still join it before destroying the owner.
    if (m_receiveThread.joinable() && m_receiveThread.get_id() == std::this_thread::get_id())
        return;
    if (m_receiveThread.joinable())
        m_receiveThread.join();
    cleanupDatagramTransports(User::Transport::boostUdp, *this);
    m_socketState->close();
}

void UDPServer::receiveFromClients()
{
    nx_data receiveBuffer(NEXILIS_BUFFER);
    boost::asio::ip::udp::endpoint remoteEndpoint;
    while (!m_stopped->load())
    {
        expireIdleDatagramTransports(User::Transport::boostUdp, *this);
        try
        {
            receiveBuffer.resize(NEXILIS_BUFFER);

            boost::system::error_code ec;
            size_t bytes_received = m_socketState->receive(receiveBuffer, remoteEndpoint, ec);
            if (ec == boost::asio::error::would_block || ec == boost::asio::error::try_again)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            if (ec)
                throw boost::system::system_error(ec);

            if (m_stopped->load())
            {
                break;
            }

            if (bytes_received == 0)
            {
                continue;
            }

            std::string address = remoteEndpoint.address().to_string();
            // Create a new buffer containing only the received bytes.
            nx_data received_message(receiveBuffer.begin(), receiveBuffer.begin() + bytes_received);

            const auto senderEndpoint = remoteEndpoint;
            const auto endpointKey = address + ":" + std::to_string(senderEndpoint.port());
            nx_data payload;
            if (udp_auth::hasPrefix(received_message, udp_auth::hello))
            {
                if (received_message.size() == udp_auth::makeHello().size())
                {
                    auto challenge = udp_auth::makeChallenge(m_cookieKey, endpointKey);
                    if (!challenge.empty())
                        m_socketState->send(senderEndpoint, challenge);
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
                auto user = ClientStorage::getClientById(Util::uint64FromFront(idBytes));
                if (!user || !udp_auth::readSession(received_message, user->getUdpToken(), payload))
                    continue;
                auto bound = m_clientEndpoints.find(user->getId());
                if (bound != m_clientEndpoints.end() && bound->second != senderEndpoint)
                    continue;
                // MessageHandler independently verifies MAC and replay IDs. Preserve
                // the signed request; readSession returns the untagged payload.
                payload.assign(received_message.begin() + 4, received_message.end());
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

            auto* user = handledMessage->getUser();
            touchDatagramTransport(*user, User::Transport::boostUdp);
            if (!user->isBoostUDPSet())
            {
                m_clientEndpoints[user->getId()] = senderEndpoint;
                user->setBoostUDPSend([socket = m_socketState, senderEndpoint](const nx_data& bytes)
                                      { socket->send(senderEndpoint, bytes); });
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
        catch (...)
        {
            // Keep serving later datagrams. Exception text may contain secrets;
            // a user-installed logger must not throw out of this boundary either.
            try
            {
                Log::error("UDP request processing failed");
            }
            catch (...)
            {
            }
        }
    }
}

} // namespace nexilis::server::nxboost
