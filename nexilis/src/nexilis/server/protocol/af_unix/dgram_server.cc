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

#ifdef __linux__

#include <nexilis/logger/log.hh>
#include <nexilis/nexilis_constants.hh>
#include <nexilis/server/command/command_result.hh>
#include <nexilis/server/message/auth_message.hh>
#include <nexilis/server/message/message.hh>
#include <nexilis/server/protocol/af_unix/dgram_server.hh>

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace nexilis::server::af_unix
{

DgramServer::DgramServer(const ServerConfig& settings, const std::string& socketPath)
    : ServerProtocol(settings),
      m_socketPath(socketPath),
      m_mutex(std::make_unique<std::mutex>()),
      m_stopped(std::make_unique<std::atomic<bool>>(false))
{
    createSocket();
    bindSocket();
}

DgramServer::~DgramServer()
{
    stop();
    unlink(m_socketPath.c_str());
}

DgramServer::DgramServer(DgramServer&& other)
    : NXProtocol(std::move(other)),
      ServerProtocol(std::move(other)),
      m_socketPath(std::move(other.m_socketPath)),
      m_serverSocket(std::move(other.m_serverSocket)),
      m_receiveThread(std::move(other.m_receiveThread)),
      m_mutex(std::move(other.m_mutex)),
      m_stopped(std::move(other.m_stopped))
{
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

DgramServer& DgramServer::operator=(DgramServer&& other)
{
    if (this != &other)
    {
        static_cast<NXProtocol&>(*this) = static_cast<NXProtocol&&>(other);
        static_cast<ServerProtocol&>(*this) = static_cast<ServerProtocol&&>(other);

        m_socketPath = std::move(other.m_socketPath);
        m_serverSocket = std::move(other.m_serverSocket);
        m_receiveThread = std::move(other.m_receiveThread);
        m_mutex = std::move(other.m_mutex);
        m_stopped = std::move(other.m_stopped);

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

void DgramServer::start()
{
    if (m_stopped->load())
    {
        Log::debug("DgramServer already stopped, cannot start");
        return;
    }

    if (m_serverSocket == -1)
    {
        Log::error("DgramServer: cannot start, socket is not open");
        return;
    }

    m_receiveThread = std::thread(&DgramServer::receiveFromClients, this);
    Log::info("DgramServer started at: ", m_socketPath);
}

void DgramServer::stop()
{
    if (!m_stopped || m_stopped->exchange(true))
    {
        Log::debug("DgramServer stop already in progress or completed");
        return;
    }

    std::unique_lock<std::mutex> lock(*m_mutex);

    if (m_serverSocket != -1)
    {
        if (close(m_serverSocket) == -1)
        {
            Log::error("Error closing server socket: ", strerror(errno));
        }
        m_serverSocket = -1;
    }
    lock.unlock();

    if (m_receiveThread.joinable())
    {
        Log::debug("DgramServer closing receiveThread");
        m_receiveThread.join();
    }

    Log::debug("DgramServer stopped");
}

void DgramServer::createSocket()
{
    m_serverSocket = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (m_serverSocket == -1)
    {
        Log::error("Error creating DgramServer socket: ", strerror(errno));
        return;
    }

    // Set a receive timeout so the receive loop can periodically check the
    // stopped flag and exit cleanly on stop() instead of blocking forever.
    timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; // 100 ms
    if (setsockopt(m_serverSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1)
    {
        Log::error("Error setting DgramServer receive timeout: ", strerror(errno));
    }
}

void DgramServer::bindSocket()
{
    sockaddr_un serverAddr{};
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sun_family = AF_UNIX;
    strcpy(serverAddr.sun_path, m_socketPath.c_str());

    // Remove old socket file. This operation will fail for first time usage.
    unlink(m_socketPath.c_str());

    auto address = reinterpret_cast<sockaddr*>(&serverAddr);
    if (bind(m_serverSocket, address, sizeof(serverAddr)) == -1)
    {
        Log::critical("Error binding DgramServer socket, reason: ", strerror(errno));
        close(m_serverSocket);
        m_serverSocket = -1;
    }
}

void DgramServer::sendToClient(const sockaddr_un& clientAddress, const nx_data& message)
{
    std::lock_guard<std::mutex> lock(*m_mutex);

    if (m_serverSocket == -1)
    {
        Log::error("DgramServer: cannot send, socket is not open");
        return;
    }

    ssize_t sentBytes = sendto(m_serverSocket, message.data(), message.size(), 0,
                               reinterpret_cast<const sockaddr*>(&clientAddress),
                               sizeof(clientAddress));

    if (sentBytes == -1)
    {
        Log::error("Failed to send message to client: ", strerror(errno));
    }
}

void DgramServer::receiveFromClients()
{
    while (!m_stopped->load() && m_serverSocket != -1)
    {
        sockaddr_un clientAddress{};
        socklen_t clientAddressLen = sizeof(clientAddress);
        nx_data buffer(NEXILIS_BUFFER);

        ssize_t bytesReceived = recvfrom(m_serverSocket, buffer.data(), buffer.size(), 0,
                                         reinterpret_cast<sockaddr*>(&clientAddress),
                                         &clientAddressLen);

        if (bytesReceived <= 0)
        {
            if (bytesReceived == -1 && errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
            {
                Log::error("Error receiving message: ", strerror(errno));
            }
            continue;
        }

        buffer.resize(bytesReceived);

        // Use the client's own socket path as the address that identifies the
        // connection, mirroring how the boost UDP server uses the IP address.
        std::string address = clientAddress.sun_path;
        Log::info("DgramServer received from ", address, " ", bytesReceived, " bytes");

        auto handledMessage = getMessageHandler().readMessage(address, buffer, &getSettings());

        if (!handledMessage || !handledMessage->getUser())
        {
            Log::error("DgramServer received invalid message or null user");
            continue;
        }

        auto* user = handledMessage->getUser();
        if (!user->isUnixDgramSet())
        {
            auto senderAddress = clientAddress;
            user->setUnixDgramSend([this, senderAddress](const nx_data& bytes)
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
                Log::info("DgramServer: Passed");
            }
        }
        else
        {
            Log::error("DgramServer: Unrecognized message type");
        }
    }
}

} // namespace nexilis::server::af_unix

#endif // __linux__
