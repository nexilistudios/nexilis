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
#include <nexilis/room_command_type.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command_result.hh>
#include <nexilis/server/command/commands.hh>
#include <nexilis/server/message/auth_message.hh>
#include <nexilis/server/message/message.hh>
#include <nexilis/server/room_storage.hh>

#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>

#include <cstring>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace nexilis::server::af_inet
{

struct TCPServer::Connection
{
    explicit Connection(int socket) : fd(socket)
    {
    }

    ~Connection()
    {
        close();
    }

    void shutdown()
    {
        std::lock_guard lock(mutex);
        acceptingSends = false;
        if (fd != -1)
            ::shutdown(fd, SHUT_RDWR);
    }

    void close()
    {
        std::lock_guard lock(mutex);
        acceptingSends = false;
        if (fd != -1)
        {
            ::shutdown(fd, SHUT_RDWR);
            ::close(fd);
            fd = -1;
        }
    }

    int socket() const
    {
        std::lock_guard lock(mutex);
        return fd;
    }

    void send(const nx_data& bytes)
    {
        std::lock_guard lock(mutex);
        if (!acceptingSends || fd == -1)
            throw std::runtime_error("IPv4 TCP connection is closed");
        if (bytes.size() > std::numeric_limits<uint32_t>::max())
            throw std::length_error("IPv4 TCP response is too large");
        const uint32_t networkSize = htonl(static_cast<uint32_t>(bytes.size()));
        const auto sendAll = [&](const void* buffer, size_t size)
        {
            const auto* data = static_cast<const char*>(buffer);
            size_t offset = 0;
            while (offset < size)
            {
                const auto sent = ::send(fd, data + offset, size - offset, MSG_NOSIGNAL | MSG_DONTWAIT);
                if (sent < 0 && errno == EINTR)
                    continue;
                if (sent <= 0)
                {
                    const int error = sent < 0 ? errno : EPIPE;
                    // A partial frame cannot be retried as a new message.
                    acceptingSends = false;
                    ::shutdown(fd, SHUT_RDWR);
                    throw std::system_error(error, std::generic_category(), "IPv4 TCP send failed");
                }
                offset += static_cast<size_t>(sent);
            }
        };
        sendAll(&networkSize, sizeof(networkSize));
        sendAll(bytes.data(), bytes.size());
    }

    // Only the receive worker closes fd, after recv has finished. stop()
    // shuts it down to wake reads. Retained callbacks cannot use a recycled fd.
    int fd;
    bool acceptingSends = true;
    mutable std::mutex mutex;
};

TCPServer::Client::Client(std::string address, uint16_t port, int socket)
    : m_address(std::move(address)),
      m_port(port),
      m_connection(std::make_shared<Connection>(socket))
{
}

int TCPServer::Client::getSocket() const
{
    return m_connection ? m_connection->socket() : -1;
}

TCPServer& TCPServer::movable(TCPServer& server)
{
    if (server.m_acceptThread.joinable())
        throw std::logic_error("Stop an IPv4 TCP server before moving it");
    for (const auto& thread : server.m_clientThreads)
    {
        if (thread.joinable())
            throw std::logic_error("Stop an IPv4 TCP server before moving it");
    }
    return server;
}

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
}

TCPServer::TCPServer(TCPServer&& other)
    : NXProtocol(std::move(movable(other))),
      ServerProtocol(std::move(other)),
      m_port(std::move(other.m_port)),
      m_serverSocket(std::move(other.m_serverSocket)),
      m_serverAddr(std::move(other.m_serverAddr)),
      m_acceptThread(std::move(other.m_acceptThread)),
      m_clientThreads(std::move(other.m_clientThreads)),
      m_clients(std::move(other.m_clients)),
      m_stopped(std::move(other.m_stopped)),
      m_mutex(std::move(other.m_mutex))
{
    other.m_serverSocket = -1;
}

TCPServer& TCPServer::operator=(TCPServer&& other)
{
    if (this != &other)
    {
        movable(other);
        for (const auto& thread : m_clientThreads)
        {
            if (thread.joinable() && thread.get_id() == std::this_thread::get_id())
                throw std::logic_error("Cannot replace an IPv4 TCP server from its worker");
        }
        stop();
        static_cast<NXProtocol&>(*this) = static_cast<NXProtocol&&>(other);
        static_cast<ServerProtocol&>(*this) = static_cast<ServerProtocol&&>(other);

        m_port = std::move(other.m_port);
        m_serverSocket = std::move(other.m_serverSocket);
        m_serverAddr = std::move(other.m_serverAddr);
        m_acceptThread = std::move(other.m_acceptThread);
        m_clientThreads = std::move(other.m_clientThreads);
        m_clients = std::move(other.m_clients);
        m_stopped = std::move(other.m_stopped);
        m_mutex = std::move(other.m_mutex);

        other.m_serverSocket = -1;
    }
    return *this;
}

void TCPServer::start()
{
    if (!m_stopped || m_stopped->load() || m_acceptThread.joinable() || m_serverSocket == -1)
        return;

    m_acceptThread = std::thread([this]()
                                 { acceptClients(); });
}

void TCPServer::stop()
{
    if (!m_stopped)
    {
        return;
    }

    m_stopped->store(true);

    // The accept loop polls the listening socket with a short timeout and
    // exits on its own once m_stopped is set. Join it before closing the
    // socket: closing an fd while another thread is blocked on it does not
    // wake the blocked syscall on Linux, and the join would hang forever.
    if (m_acceptThread.joinable())
    {
        m_acceptThread.join();
    }

    // Disable sends and wake reads; each worker closes its own connection.
    {
        std::lock_guard<std::mutex> lock(*m_mutex);
        for (auto& client : m_clients)
        {
            if (client)
            {
                client->m_connection->shutdown();
            }
        }
    }

    // A dispatch callback can request shutdown. The controlling thread must
    // call stop() again to join workers before destruction or moving.
    for (const auto& thread : m_clientThreads)
    {
        if (thread.joinable() && thread.get_id() == std::this_thread::get_id())
            return;
    }
    for (auto& thread : m_clientThreads)
    {
        if (thread.joinable())
            thread.join();
    }
    m_clientThreads.clear();
    m_clients.clear();

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

        std::shared_ptr<Client> client;
        try
        {
            std::string clientAddress = inet_ntoa(clientAddr.sin_addr);
            uint16_t clientPort = ntohs(clientAddr.sin_port);
            client = std::make_shared<Client>(clientAddress, clientPort, clientSocket);
            handleClient(client);
        }
        catch (...)
        {
            // Allocation or worker creation failure must not leak the accepted
            // descriptor or escape the accept thread.
            if (!client)
                ::close(clientSocket);
        }
    }
}

void TCPServer::handleClient(const std::shared_ptr<Client>& client)
{
    std::lock_guard<std::mutex> lock(*m_mutex);
    m_clients.push_back(client);
    connectionEstablished();

    try
    {
        m_clientThreads.emplace_back(&TCPServer::receiveFromClient, this, client);
    }
    catch (...)
    {
        connectionClosed();
        m_clients.pop_back();
        throw;
    }
}

void TCPServer::receiveFromClient(const std::shared_ptr<Client>& client)
{
    const auto connection = client->m_connection;
    const int clientSocket = connection->socket();
    uint64_t connectedUserId = 0;
    try
    {
        nx_data receivedData;

        while (!m_stopped->load())
        {
            if (!receiveMessage(clientSocket, receivedData))
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
            if (connectedUserId == 0)
                connectedUserId = user->getId();
            if (!user->isInetTCPSet())
            {
                user->setInetTCPSend([connection = client->m_connection](const nx_data& bytes)
                                     { connection->send(bytes); });
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
                    Log::debug("TCPServer: Passed");
                }
            }
            else
            {
                Log::error("TCPServer: Unrecognized message type");
            }
        }
    }
    catch (...)
    {
        // Parsing or application callbacks must not escape a worker.
    }

    // --- Cleanup: remove disconnected user from their room and client storage ---
    if (connectedUserId != 0)
    {
        auto user = ClientStorage::getClientById(connectedUserId);
        if (user)
        {
            uint64_t roomId = user->getRoomId();
            if (roomId != 0)
            {
                auto room = RoomStorage::getRoomById(roomId);
                if (room)
                {
                    nx_data leaveData = {
                            static_cast<uint8_t>(CommandType::room),
                            static_cast<uint8_t>(RoomCommandType::Root::management),
                            static_cast<uint8_t>(RoomCommandType::Management::leave)};
                    DefaultArgs args(*user, *this, leaveData, 0);
                    ServerImpl::room_management_leave(args);
                }
            }
            ClientStorage::remove(connectedUserId);
        }
    }

    connection->close();
    connectionClosed();
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
