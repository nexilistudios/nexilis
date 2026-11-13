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

#include <nexilis/server/protocol/af_inet/udp_server.hh>

#include <nexilis/logger/log.hh>
#include <nexilis/nexilis_constants.hh>
#include <nexilis/ports.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command_result.hh>
#include <nexilis/server/message/auth_message.hh>
#include <nexilis/server/message/message.hh>
#include <nexilis/udp_auth.hh>
#include <nexilis/util.hh>

#include <openssl/rand.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <system_error>

namespace nexilis::server::af_inet
{

struct UDPServer::SocketState
{
    ~SocketState()
    {
        close();
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
        if (fd != -1)
        {
            ::close(fd);
            fd = -1;
        }
    }

    void send(const sockaddr_in& address, const nx_data& bytes)
    {
        std::lock_guard lock(mutex);
        if (!acceptingSends || fd == -1)
            throw std::runtime_error("IPv4 UDP socket is closed");
        if (sendto(fd, bytes.data(), bytes.size(), MSG_DONTWAIT | MSG_NOSIGNAL,
                   reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == -1)
            throw std::system_error(errno, std::generic_category(), "IPv4 UDP send failed");
    }

    // stop() joins the receiver before changing or closing the descriptor.
    int fd = -1;
    bool acceptingSends = true;
    std::mutex mutex;
};

UDPServer& UDPServer::movable(UDPServer& protocol)
{
    if (protocol.m_receiveThread.joinable())
        throw std::logic_error("Stop an IPv4 UDP server before moving it");
    return protocol;
}

UDPServer::UDPServer(const ServerConfig& settings, uint16_t port)
    : ServerProtocol(settings),
      m_port(port == 0 ? Ports::getInetUDPPort() : port),
      m_socketState(std::make_shared<SocketState>()),
      m_stopped(std::make_unique<std::atomic<bool>>(false))
{
    createSocket();
    bindSocket();
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
      m_port(std::move(other.m_port)),
      m_socketState(std::move(other.m_socketState)),
      m_serverAddr(std::move(other.m_serverAddr)),
      m_receiveThread(std::move(other.m_receiveThread)),
      m_stopped(std::move(other.m_stopped))
{
    m_cookieKey = other.m_cookieKey;
    m_clientEndpoints = std::move(other.m_clientEndpoints);
    if (!m_stopped)
    {
        m_stopped = std::make_unique<std::atomic<bool>>(false);
    }
}

UDPServer& UDPServer::operator=(UDPServer&& other)
{
    if (this != &other)
    {
        movable(other);
        stop();
        static_cast<NXProtocol&>(*this) = static_cast<NXProtocol&&>(other);
        static_cast<ServerProtocol&>(*this) = static_cast<ServerProtocol&&>(other);

        m_port = std::move(other.m_port);
        m_socketState = std::move(other.m_socketState);
        m_serverAddr = std::move(other.m_serverAddr);
        m_receiveThread = std::move(other.m_receiveThread);
        m_stopped = std::move(other.m_stopped);
        m_cookieKey = other.m_cookieKey;
        m_clientEndpoints = std::move(other.m_clientEndpoints);

        if (!m_stopped)
        {
            m_stopped = std::make_unique<std::atomic<bool>>(false);
        }
    }
    return *this;
}

void UDPServer::start()
{
    if (!m_stopped || m_stopped->load() || m_receiveThread.joinable())
    {
        Log::debug("UDPServer already stopped, cannot start");
        return;
    }

    if (m_socketState->fd == -1)
    {
        Log::error("UDPServer: cannot start, socket is not open");
        return;
    }

    // Announce the port so clients can discover it, mirroring the boost UDP
    // server behaviour.
    if (!Util::writePortToFile(m_port, getType()))
    {
        Log::error("UDPServer: Failed to write server port to a file");
    }

    m_receiveThread = std::thread(&UDPServer::receiveFromClients, this);
    Log::info("UDPServer started on port: ", m_port);
}

void UDPServer::stop()
{
    if (!m_stopped || m_stopped->exchange(true))
    {
        Log::debug("UDPServer stop already in progress or completed");
        return;
    }

    m_socketState->disableSends();
    if (m_receiveThread.joinable())
        m_receiveThread.join();
    cleanupDatagramTransports(User::Transport::inetUdp, *this);
    m_socketState->close();

    Log::debug("UDPServer stopped");
}

void UDPServer::createSocket()
{
    m_socketState->fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (m_socketState->fd == -1)
    {
        throw std::system_error(errno, std::generic_category(), "IPv4 UDP socket creation failed");
    }

    int opt = 1;
    if (setsockopt(m_socketState->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        Log::error("UDPServer: Error setting SO_REUSEADDR: ", strerror(errno));
    }

    // Set a receive timeout so the receive loop can periodically check the
    // stopped flag and exit cleanly on stop() instead of blocking forever.
    timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; // 100 ms
    if (setsockopt(m_socketState->fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "IPv4 UDP receive timeout failed");
    }

    memset(&m_serverAddr, 0, sizeof(m_serverAddr));
    m_serverAddr.sin_family = AF_INET;
    m_serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    m_serverAddr.sin_port = htons(m_port);
}

void UDPServer::bindSocket()
{
    if (bind(m_socketState->fd, reinterpret_cast<sockaddr*>(&m_serverAddr), sizeof(m_serverAddr)) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "IPv4 UDP bind failed");
    }
}

void UDPServer::receiveFromClients()
{
    while (!m_stopped->load() && m_socketState->fd != -1)
    {
        expireIdleDatagramTransports(User::Transport::inetUdp, *this);
        try
        {
            sockaddr_in clientAddress{};
            socklen_t clientAddressLen = sizeof(clientAddress);
            nx_data buffer(NEXILIS_BUFFER);

            ssize_t bytesReceived = recvfrom(m_socketState->fd, buffer.data(), buffer.size(), 0,
                                             reinterpret_cast<sockaddr*>(&clientAddress),
                                             &clientAddressLen);

            if (bytesReceived <= 0)
            {
                if (bytesReceived == -1 && errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
                {
                    Log::error("UDPServer: Error receiving message: ", strerror(errno));
                }
                continue;
            }

            buffer.resize(bytesReceived);

            char addressBuffer[INET_ADDRSTRLEN];
            const char* address = inet_ntop(AF_INET, &clientAddress.sin_addr, addressBuffer, INET_ADDRSTRLEN);
            std::string clientAddressString = address ? address : "";
            Log::info("UDPServer received from ", clientAddressString, " ", bytesReceived, " bytes");

            const auto endpointKey = clientAddressString + ":" + std::to_string(ntohs(clientAddress.sin_port));
            nx_data payload;
            if (udp_auth::hasPrefix(buffer, udp_auth::hello))
            {
                if (buffer.size() == udp_auth::makeHello().size())
                {
                    auto challenge = udp_auth::makeChallenge(m_cookieKey, endpointKey);
                    if (!challenge.empty())
                        m_socketState->send(clientAddress, challenge);
                }
                continue;
            }
            if (udp_auth::hasPrefix(buffer, udp_auth::authentication))
            {
                if (!udp_auth::checkAuthentication(buffer, m_cookieKey, endpointKey, payload))
                    continue;
                if (payload.size() >= 8 && ClientStorage::getClientById(Util::uint64FromFront(payload)))
                    continue;
            }
            else if (udp_auth::hasPrefix(buffer, udp_auth::session))
            {
                if (buffer.size() < 4 + 8)
                    continue;
                nx_data idBytes(buffer.begin() + 4, buffer.begin() + 12);
                auto user = ClientStorage::getClientById(Util::uint64FromFront(idBytes));
                if (!user || !udp_auth::readSession(buffer, user->getUdpToken(), payload))
                    continue;
                auto bound = m_clientEndpoints.find(user->getId());
                if (bound != m_clientEndpoints.end() &&
                    (bound->second.sin_addr.s_addr != clientAddress.sin_addr.s_addr ||
                     bound->second.sin_port != clientAddress.sin_port))
                    continue;
                // MessageHandler independently verifies MAC and replay IDs. Preserve
                // the signed request; readSession returns the untagged payload.
                payload.assign(buffer.begin() + 4, buffer.end());
            }
            else
            {
                continue;
            }

            auto handledMessage = getMessageHandler().readMessage(clientAddressString, payload, &getSettings());

            if (!handledMessage || !handledMessage->getUser())
            {
                Log::error("UDPServer received invalid message or null user");
                continue;
            }

            auto* user = handledMessage->getUser();
            touchDatagramTransport(*user, User::Transport::inetUdp);
            if (!user->isInetUDPSet())
            {
                m_clientEndpoints[user->getId()] = clientAddress;
                auto senderAddress = clientAddress;
                user->setInetUDPSend([socket = m_socketState, senderAddress](const nx_data& bytes)
                                     { socket->send(senderAddress, bytes); });
            }

            auto msgType = handledMessage->getType();

            if (msgType == BaseMessage::Type::auth_message)
            {
                auto authPtr = static_cast<AuthMessage*>(handledMessage.get());
                for (auto&& commandData : authPtr->getData())
                {
                    CommandResult result =
                            getCommand().read(commandData, *authPtr->getUser(), *this,
                                              authPtr->getMessageId());
                    checkResult(result);
                }
            }
            else if (msgType == BaseMessage::Type::message)
            {
                auto msgPtr = static_cast<Message*>(handledMessage.get());
                CommandResult result =
                        getCommand().read(msgPtr->getData(), *msgPtr->getUser(), *this,
                                          msgPtr->getMessageId());

                checkResult(result);

                if (result == CommandResult::success)
                {
                    Log::debug("UDPServer: Passed");
                }
            }
            else
            {
                Log::error("UDPServer: Unrecognized message type");
            }
        }
        catch (...)
        {
            // Shutdown or a failed send must not escape the worker thread.
            try
            {
                Log::error("IPv4 UDP request processing failed");
            }
            catch (...)
            {
            }
        }
    }
}

} // namespace nexilis::server::af_inet
