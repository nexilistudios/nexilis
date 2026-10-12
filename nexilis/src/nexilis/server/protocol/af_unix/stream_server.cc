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

#include <nexilis/nexilis_constants.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/command/command_result.hh>
#include <nexilis/server/message/auth_message.hh>
#include <nexilis/server/protocol/af_unix/stream_server.hh>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

namespace nexilis::server::af_unix
{

StreamServer::StreamServer(const ServerConfig& settings, const std::string& socketPath)
    : ServerProtocol(settings),
      m_socketPath(socketPath),
      m_buffer(NEXILIS_BUFFER)
{
    createSocket();
    bindSocket();
}

StreamServer::~StreamServer()
{
    if (m_receiveThread.joinable())
    {
        m_receiveThread.join();
    }

    if (m_serverSocket != -1)
    {
        close(m_serverSocket);
    }

    // Remove the filesystem socket entry so no stale file convinces a client
    // (or a readiness check) that the server is still listening.
    unlink(m_socketPath.c_str());
}

StreamServer::StreamServer(StreamServer&& other)
    : NXProtocol(std::move(other)),
      ServerProtocol(std::move(other)),
      m_socketPath(std::move(other.m_socketPath)),
      m_serverSocket(std::move(other.m_serverSocket)),
      m_buffer(std::move(std::move(other.m_buffer))),
      m_receiveThread(std::move(other.m_receiveThread))
{
}

StreamServer& StreamServer::operator=(StreamServer&& other)
{
    if (this != &other)
    {
        static_cast<NXProtocol&>(*this) = static_cast<NXProtocol&&>(other);
        static_cast<ServerProtocol&>(*this) = static_cast<ServerProtocol&&>(other);

        m_socketPath = std::move(other.m_socketPath);
        m_serverSocket = std::move(other.m_serverSocket);
        m_buffer = std::move(other.m_buffer);
        m_receiveThread = std::move(other.m_receiveThread);
    }
    return *this;
}

void StreamServer::start()
{
    m_receiveThread = std::thread([this]()
                                  {
        while (true)
        {
            handleMessages();
        } });
}

void StreamServer::createSocket()
{
    m_serverSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_serverSocket == -1)
    {
        Log::error("Couldn't create socket");
    }
}

void StreamServer::bindSocket()
{
    sockaddr_un serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sun_family = AF_UNIX;
    strcpy(serverAddr.sun_path, m_socketPath.c_str());

    // Remove old socket file.
    // This operation will fail if this is the first usage and it's okay.
    unlink(m_socketPath.c_str());

    auto address = reinterpret_cast<sockaddr*>(&serverAddr);
    if (bind(m_serverSocket, address, sizeof(serverAddr)) == -1)
    {
        Log::error("Failed to bind socket");
        close(m_serverSocket);
    }

    if (listen(m_serverSocket, 30) == -1)
    {
        Log::error("Failed to listen to socket");
        close(m_serverSocket);
    }
}

void StreamServer::sendMessage(int clientSocket, const nx_data& message)
{
    ssize_t sentBytes = send(clientSocket, message.data(), message.size(), 0);

    if (sentBytes == -1)
    {
        Log::error("Failed to send message");
    }
}

std::string StreamServer::receiveMessage(int socket)
{
    std::string message;
    char buffer[NEXILIS_BUFFER];

    while (true)
    {
        auto bytesRead = recv(socket, buffer, sizeof(buffer), 0);

        if (bytesRead > 0)
        {
            message.append(buffer, bytesRead);

            // Check if the message contains the null terminator.
            size_t nullPos = message.find('\0');

            if (nullPos != std::string::npos)
            {
                return message.substr(0, nullPos);
            }
            else
            {
                Log::error("Received message that does",
                           " not contain the null-termination character");
                break;
            }
        }

        else if (bytesRead == 0)
        {
            Log::info("Connection closed by peer");
            break;
        }
        else
        {
            Log::error("Error receiving message");
            break;
        }
    }
    return "";
}

void StreamServer::handleMessages()
{
    int clientSocket = accept(m_serverSocket, nullptr, nullptr);
    if (clientSocket == -1)
    {
        Log::error("Failed to accept connection");
        return;
    }

    while (true)
    {
        std::string message = receiveMessage(clientSocket);

        if (message == "")
        {
            break;
        }

        nx_data payload = Util::convertToByteVector(message);
        auto handledMessage = getMessageHandler().readMessage("127.0.0.1", payload, &getSettings());

        if (!handledMessage->getUser())
        {
            Log::error("Message from unauthorized client!");
            break;
        }

        auto* user = handledMessage->getUser();
        if (!user->isUnixStreamSet())
        {
            user->setUnixStreamSend([this, clientSocket](const nx_data& bytes)
                                    { sendMessage(clientSocket, bytes); });
        }

        auto type = handledMessage->getType();

        if (type == BaseMessage::Type::auth_message)
        {
            // Reply to the handshake so the client receives its client id.
            auto msgPtr = static_cast<AuthMessage*>(handledMessage.get());
            for (auto&& commandData : msgPtr->getData())
            {
                CommandResult result =
                        getCommand().read(commandData, *user, *this, msgPtr->getMessageId());
                checkResult(result);
            }
        }
        else if (type == BaseMessage::Type::message)
        {
            auto msgPtr = static_cast<Message*>(handledMessage.get());
            CommandResult result =
                    getCommand().read(msgPtr->getData(), *user, *this, msgPtr->getMessageId());

            checkResult(result);

            if (result == CommandResult::success)
            {
                Log::info("Passed");
            }
        }
    }
}

} // namespace nexilis::server::af_unix

#endif
