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

#include <nexilis/server/protocol/af_inet/tcp_server.hh>

#include <nexilis/logger/log.hh>
#include <nexilis/nexilis_constants.hh>
#include <nexilis/ports.hh>
#include <nexilis/server/command/command_result.hh>
#include <nexilis/server/message/auth_message.hh>
#include <nexilis/server/message/message.hh>

#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>

#include <cstring>

namespace nexilis::server::af_inet
{

TCPServer::TCPServer(const ServerConfig& settings, uint16_t port)
    : NXProtocol(),
      ServerProtocol(settings),
      m_port(port == 0 ? Ports::getInetTCPPort() : port),
      m_stopped(std::make_unique<std::atomic<bool>>(false)),
      m_mutex(std::make_unique<std::mutex>())
{
    createSocket();
    bindSocket();
    listenSocket();
}

TCPServer::~TCPServer()
{
    stop();

    if (m_acceptThread.joinable())
    {
        m_acceptThread.join();
    }

    for (auto& clientThread : m_clientThreads)
    {
        if (clientThread.joinable())
        {
            clientThread.join();
        }
    }
}

TCPServer::TCPServer(TCPServer&& other)
    : NXProtocol(std::move(other)),
      ServerProtocol(std::move(other)),
      m_port(std::move(other.m_port)),
      m_serverSocket(std::move(other.m_serverSocket)),
      m_serverAddr(std::move(other.m_serverAddr)),
      m_acceptThread(std::move(other.m_acceptThread)),
      m_clientThreads(std::move(other.m_clientThreads)),
      m_stopped(std::move(other.m_stopped)),
      m_mutex(std::move(other.m_mutex))
{
    other.m_serverSocket = -1;
}

TCPServer& TCPServer::operator=(TCPServer&& other)
{
    if (this != &other)
    {
        static_cast<NXProtocol&>(*this) = static_cast<NXProtocol&&>(other);
        static_cast<ServerProtocol&>(*this) = static_cast<ServerProtocol&&>(other);

        m_port = std::move(other.m_port);
        m_serverSocket = std::move(other.m_serverSocket);
        m_serverAddr = std::move(other.m_serverAddr);
        m_acceptThread = std::move(other.m_acceptThread);
        m_clientThreads = std::move(other.m_clientThreads);
        m_stopped = std::move(other.m_stopped);
        m_mutex = std::move(other.m_mutex);

        other.m_serverSocket = -1;
    }
    return *this;
}

void TCPServer::start()
{
    if (m_stopped->load())
    {
        m_stopped->store(false);
    }

    m_acceptThread = std::thread([this]()
                                 { acceptClients(); });
}

void TCPServer::stop()
{
    if (!m_stopped || m_stopped->exchange(true))
    {
        return;
    }

    // The accept loop polls the listening socket with a short timeout and
    // exits on its own once m_stopped is set. Join it before closing the
    // socket: closing an fd while another thread is blocked on it does not
    // wake the blocked syscall on Linux, and the join would hang forever.
    if (m_acceptThread.joinable())
    {
        m_acceptThread.join();
    }

    // Close the client sockets so the client handler threads unblock with EOF.
    {
        std::lock_guard<std::mutex> lock(*m_mutex);
        for (auto& client : m_clients)
        {
            if (client)
            {
                shutdown(client->getSocket(), SHUT_RDWR);
            }
        }
    }

    if (m_serverSocket != -1)
    {
        close(m_serverSocket);
        m_serverSocket = -1;
    }
}

void TCPServer::createSocket()
{
    m_serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (m_serverSocket == -1)
    {
        Log::error("TCPServer: Error creating socket: ", std::strerror(errno));
        return;
    }

    int opt = 1;
    if (setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        Log::error("TCPServer: Error setting SO_REUSEADDR: ", std::strerror(errno));
    }

    std::memset(&m_serverAddr, 0, sizeof(m_serverAddr));
    m_serverAddr.sin_family = AF_INET;
    m_serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    m_serverAddr.sin_port = htons(m_port);
}

void TCPServer::bindSocket()
{
    if (bind(m_serverSocket, reinterpret_cast<sockaddr*>(&m_serverAddr), sizeof(m_serverAddr)) == -1)
    {
        Log::error("TCPServer: Error binding socket: ", std::strerror(errno));
        close(m_serverSocket);
        m_serverSocket = -1;
    }
}

void TCPServer::listenSocket()
{
    if (listen(m_serverSocket, SOMAXCONN) == -1)
    {
        Log::error("TCPServer: Error listening on socket: ", std::strerror(errno));
        close(m_serverSocket);
        m_serverSocket = -1;
    }
}

void TCPServer::acceptClients()
{
    while (!m_stopped->load())
    {
        pollfd pfd{};
        pfd.fd = m_serverSocket;
        pfd.events = POLLIN;

        // Poll with a short timeout so that a pending stop is detected even
        // while accept() would otherwise block indefinitely.
        int ready = poll(&pfd, 1, 100);

        if (ready <= 0)
        {
            continue;
        }

        sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);

        int clientSocket = accept(m_serverSocket, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);

        if (clientSocket == -1)
        {
            if (!m_stopped->load())
            {
                Log::error("TCPServer: Error accepting client: ", std::strerror(errno));
            }
            continue;
        }

        std::string clientAddress = inet_ntoa(clientAddr.sin_addr);
        uint16_t clientPort = ntohs(clientAddr.sin_port);
        auto client = std::make_shared<Client>(clientAddress, clientPort, clientSocket);
        handleClient(client);
    }
}

void TCPServer::handleClient(const std::shared_ptr<Client>& client)
{
    std::lock_guard<std::mutex> lock(*m_mutex);
    m_clients.push_back(client);
    connectionEstablished();

    m_clientThreads.emplace_back([this, client]()
                                 {
                                     nx_data receivedData;

                                     while (!m_stopped->load())
                                     {
                                         if (!receiveMessage(client->getSocket(), receivedData))
                                         {
                                             break;
                                         }

                                         auto handledMessage = getMessageHandler().readMessage(
                                                 client->getAddress(), receivedData, &getSettings());

                                         if (!handledMessage || !handledMessage->getUser())
                                         {
                                             Log::error("TCPServer received invalid message or null user");
                                             continue;
                                         }

                                         auto* user = handledMessage->getUser();
                                         if (!user->isInetTCPSet())
                                         {
                                             int clientSocket = client->getSocket();
                                             user->setInetTCPSend([this, clientSocket](const nx_data& bytes)
                                                                  {
                                                                      std::string message(bytes.begin(), bytes.end());
                                                                      sendMessageToClient(message, clientSocket);
                                                                  });
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
                                                 Log::info("TCPServer: Passed");
                                             }
                                         }
                                         else
                                         {
                                             Log::error("TCPServer: Unrecognized message type");
                                         }
                                     }

                                     connectionClosed(); });
}

std::shared_ptr<TCPServer::Client> TCPServer::findClient(int clientSocket)
{
    std::lock_guard<std::mutex> lock(*m_mutex);
    for (auto& client : m_clients)
    {
        if (client->getSocket() == clientSocket)
        {
            return client;
        }
    }
    return nullptr;
}

bool TCPServer::sendMessageToClient(const std::string& message, int clientSocket)
{
    std::lock_guard<std::mutex> lock(*m_mutex);

    std::shared_ptr<Client> client;
    for (auto& candidate : m_clients)
    {
        if (candidate->getSocket() == clientSocket)
        {
            client = candidate;
            break;
        }
    }

    if (!client)
    {
        Log::error("TCPServer: Unknown client socket: ", clientSocket);
        return false;
    }

    uint32_t frameSize = static_cast<uint32_t>(message.size());
    uint32_t networkSize = htonl(frameSize);

    if (send(client->getSocket(), &networkSize, sizeof(networkSize), MSG_NOSIGNAL) == -1)
    {
        Log::error("TCPServer: Error sending frame size: ", std::strerror(errno));
        return false;
    }

    const char* data = message.data();
    size_t remaining = message.size();

    while (remaining > 0)
    {
        ssize_t sent = ::send(client->getSocket(), data, remaining, MSG_NOSIGNAL);
        if (sent == -1)
        {
            Log::error("TCPServer: Error sending message: ", std::strerror(errno));
            return false;
        }
        data += sent;
        remaining -= static_cast<size_t>(sent);
    }

    return true;
}

bool TCPServer::receiveMessage(int clientSocket, nx_data& receivedData)
{
    uint32_t networkSize = 0;
    ssize_t received = recv(clientSocket, &networkSize, sizeof(networkSize), 0);

    if (received <= 0)
    {
        return false;
    }

    if (received != static_cast<ssize_t>(sizeof(networkSize)))
    {
        Log::error("TCPServer: Error receiving frame size");
        return false;
    }

    uint32_t frameSize = ntohl(networkSize);

    if (frameSize > NEXILIS_BUFFER)
    {
        Log::error("TCPServer: Invalid frame size received: ", frameSize);
        return false;
    }

    receivedData.resize(frameSize);
    size_t bytesReceived = 0;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    while (bytesReceived < frameSize)
    {
        ssize_t read = recv(clientSocket, receivedData.data() + bytesReceived, frameSize - bytesReceived, 0);

        if (read <= 0)
        {
            return false;
        }

        bytesReceived += static_cast<size_t>(read);
    }
#pragma GCC diagnostic pop

    return true;
}

} // namespace nexilis::server::af_inet
