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

namespace nexilis::server::af_inet
{

UDPServer::UDPServer(const ServerConfig& settings, uint16_t port)
    : ServerProtocol(settings),
      m_port(port == 0 ? Ports::getInetUDPPort() : port),
      m_mutex(std::make_unique<std::mutex>()),
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
    : NXProtocol(std::move(other)),
      ServerProtocol(std::move(other)),
      m_port(std::move(other.m_port)),
      m_serverSocket(std::move(other.m_serverSocket)),
      m_serverAddr(std::move(other.m_serverAddr)),
      m_receiveThread(std::move(other.m_receiveThread)),
      m_mutex(std::move(other.m_mutex)),
      m_stopped(std::move(other.m_stopped))
{
    m_cookieKey = other.m_cookieKey;
    m_clientEndpoints = std::move(other.m_clientEndpoints);
    other.m_serverSocket = -1;
    if (!m_mutex)
    {
        m_mutex = std::make_unique<std::mutex>();
    }
    if (!m_stopped)
    {
        m_stopped = std::make_unique<std::atomic<bool>>(false);
    }
}

UDPServer& UDPServer::operator=(UDPServer&& other)
{
    if (this != &other)
    {
        static_cast<NXProtocol&>(*this) = static_cast<NXProtocol&&>(other);
        static_cast<ServerProtocol&>(*this) = static_cast<ServerProtocol&&>(other);

        m_port = std::move(other.m_port);
        m_serverSocket = std::move(other.m_serverSocket);
        m_serverAddr = std::move(other.m_serverAddr);
        m_receiveThread = std::move(other.m_receiveThread);
        m_mutex = std::move(other.m_mutex);
        m_stopped = std::move(other.m_stopped);
        m_cookieKey = other.m_cookieKey;
        m_clientEndpoints = std::move(other.m_clientEndpoints);

        other.m_serverSocket = -1;
        if (!m_mutex)
        {
            m_mutex = std::make_unique<std::mutex>();
        }
        if (!m_stopped)
        {
            m_stopped = std::make_unique<std::atomic<bool>>(false);
        }
    }
    return *this;
}

void UDPServer::start()
{
    if (m_stopped->load())
    {
        Log::debug("UDPServer already stopped, cannot start");
        return;
    }

    if (m_serverSocket == -1)
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

    std::unique_lock<std::mutex> lock(*m_mutex);

    if (m_serverSocket != -1)
    {
        if (close(m_serverSocket) == -1)
        {
            Log::error("UDPServer: Error closing server socket: ", strerror(errno));
        }
        m_serverSocket = -1;
    }
    lock.unlock();

    if (m_receiveThread.joinable())
    {
        Log::debug("UDPServer closing receiveThread");
        m_receiveThread.join();
    }

    Log::debug("UDPServer stopped");
}

void UDPServer::createSocket()
{
    m_serverSocket = socket(AF_INET, SOCK_DGRAM, 0);

    if (m_serverSocket == -1)
    {
        Log::error("UDPServer: Error creating socket: ", strerror(errno));
        return;
    }

    int opt = 1;
    if (setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        Log::error("UDPServer: Error setting SO_REUSEADDR: ", strerror(errno));
    }

    // Set a receive timeout so the receive loop can periodically check the
    // stopped flag and exit cleanly on stop() instead of blocking forever.
    timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; // 100 ms
    if (setsockopt(m_serverSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1)
    {
        Log::error("UDPServer: Error setting receive timeout: ", strerror(errno));
    }

    memset(&m_serverAddr, 0, sizeof(m_serverAddr));
    m_serverAddr.sin_family = AF_INET;
    m_serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    m_serverAddr.sin_port = htons(m_port);
}

void UDPServer::bindSocket()
{
    if (bind(m_serverSocket, reinterpret_cast<sockaddr*>(&m_serverAddr), sizeof(m_serverAddr)) == -1)
    {
        Log::error("UDPServer: Error binding socket: ", strerror(errno));
        close(m_serverSocket);
        m_serverSocket = -1;
        return;
    }
}

void UDPServer::sendToClient(const sockaddr_in& clientAddress, const nx_data& message)
{
    std::lock_guard<std::mutex> lock(*m_mutex);

    if (m_serverSocket == -1)
    {
        Log::error("UDPServer: cannot send, socket is not open");
        return;
    }

    ssize_t sentBytes = sendto(m_serverSocket, message.data(), message.size(), 0,
                               reinterpret_cast<const sockaddr*>(&clientAddress),
                               sizeof(clientAddress));

    if (sentBytes == -1)
    {
        Log::error("UDPServer: Failed to send message to client: ", strerror(errno));
    }
}

void UDPServer::receiveFromClients()
{
    while (!m_stopped->load() && m_serverSocket != -1)
    {
        sockaddr_in clientAddress{};
        socklen_t clientAddressLen = sizeof(clientAddress);
        nx_data buffer(NEXILIS_BUFFER);

        ssize_t bytesReceived = recvfrom(m_serverSocket, buffer.data(), buffer.size(), 0,
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
                    sendToClient(clientAddress, challenge);
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
            auto* user = ClientStorage::getClientById(Util::uint64FromFront(idBytes));
            if (!user || !udp_auth::readSession(buffer, user->getUdpToken(), payload))
                continue;
            auto bound = m_clientEndpoints.find(user->getId());
            if (bound != m_clientEndpoints.end() &&
                (bound->second.sin_addr.s_addr != clientAddress.sin_addr.s_addr ||
                 bound->second.sin_port != clientAddress.sin_port))
                continue;
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
        if (!user->isInetUDPSet())
        {
            m_clientEndpoints[user->getId()] = clientAddress;
            auto senderAddress = clientAddress;
            user->setInetUDPSend([this, senderAddress](const nx_data& bytes)
                                 { sendToClient(senderAddress, bytes); });
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
}

} // namespace nexilis::server::af_inet
