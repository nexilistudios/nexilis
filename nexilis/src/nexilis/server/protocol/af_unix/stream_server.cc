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
#include <nexilis/room_command_type.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/command/command_result.hh>
#include <nexilis/server/command/commands.hh>
#include <nexilis/server/message/auth_message.hh>
#include <nexilis/server/room_storage.hh>
#include <nexilis/server/protocol/af_unix/stream_server.hh>
#include <nexilis/unix_socket.hh>
#include <utility>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

namespace nexilis::server::af_unix
{

struct StreamServer::Connection
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
        open = false;
        if (fd != -1)
            ::shutdown(fd, SHUT_RDWR);
    }
    void close()
    {
        std::lock_guard lock(mutex);
        open = false;
        if (fd != -1)
        {
            ::shutdown(fd, SHUT_RDWR);
            ::close(fd);
            fd = -1;
        }
    }
    void send(const nx_data& bytes)
    {
        std::lock_guard lock(mutex);
        if (!open)
            throw std::runtime_error("Unix stream connection is closed");
        std::size_t offset = 0;
        while (offset < bytes.size())
        {
            const auto sent = ::send(fd, bytes.data() + offset, bytes.size() - offset, MSG_NOSIGNAL | MSG_DONTWAIT);
            if (sent < 0 && errno == EINTR)
                continue;
            if (sent <= 0)
            {
                open = false;
                ::shutdown(fd, SHUT_RDWR);
                throw std::runtime_error("Unix stream send failed");
            }
            offset += static_cast<std::size_t>(sent);
        }
    }
    // Only the receiving worker closes fd. stop() only shuts it down to wake
    // recv; stale send callbacks retain this state and cannot use a recycled fd.
    int fd;
    bool open = true;
    std::mutex mutex;
};

StreamServer::StreamServer(const ServerConfig& settings, const std::string& socketPath)
    : ServerProtocol(settings), m_socketPath(socketPath)
{
    unix_socket::address(m_socketPath);
    createSocket();
    try
    {
        bindSocket();
    }
    catch (...)
    {
        ::close(m_serverSocket);
        m_serverSocket = -1;
        if (m_bound)
            unlink(m_socketPath.c_str());
        throw;
    }
}

StreamServer::~StreamServer()
{
    stop();
    if (m_bound)
        unlink(m_socketPath.c_str());
}

StreamServer& StreamServer::movable(StreamServer& server)
{
    if (server.m_receiveThread.joinable())
        throw std::logic_error("Stop a Unix stream server before moving it");
    return server;
}

StreamServer::StreamServer(StreamServer&& other)
    : NXProtocol(std::move(movable(other))),
      ServerProtocol(std::move(other)),
      m_socketPath(std::move(other.m_socketPath)),
      m_serverSocket(std::exchange(other.m_serverSocket, -1)),
      m_bound(std::exchange(other.m_bound, false)),
      m_stopped(std::move(other.m_stopped)),
      m_mutex(std::move(other.m_mutex)),
      m_client(std::move(other.m_client))
{
}

StreamServer& StreamServer::operator=(StreamServer&& other)
{
    if (this != &other)
    {
        movable(other);
        stop();
        if (m_bound)
            unlink(m_socketPath.c_str());
        static_cast<NXProtocol&>(*this) = std::move(static_cast<NXProtocol&>(other));
        static_cast<ServerProtocol&>(*this) = std::move(static_cast<ServerProtocol&>(other));
        m_socketPath = std::move(other.m_socketPath);
        m_serverSocket = std::exchange(other.m_serverSocket, -1);
        m_bound = std::exchange(other.m_bound, false);
        m_stopped = std::move(other.m_stopped);
        m_mutex = std::move(other.m_mutex);
        m_client = std::move(other.m_client);
    }
    return *this;
}

void StreamServer::start()
{
    if (!m_stopped || m_stopped->load() || m_receiveThread.joinable() || m_serverSocket == -1)
        return;
    m_receiveThread = std::thread([this]
                                  {
        while (!m_stopped->load())
        {
            try { handleMessages(); }
            catch (...) { /* Drop this connection and continue accepting. */ }
        } });
}

void StreamServer::stop()
{
    if (!m_stopped)
        return;
    m_stopped->store(true);
    {
        std::lock_guard lock(*m_mutex);
        if (m_serverSocket != -1)
            ::shutdown(m_serverSocket, SHUT_RDWR);
        if (m_client)
            m_client->shutdown();
    }
    if (m_receiveThread.joinable())
    {
        if (m_receiveThread.get_id() == std::this_thread::get_id())
            return;
        m_receiveThread.join();
    }
    if (m_serverSocket != -1)
    {
        ::close(m_serverSocket);
        m_serverSocket = -1;
    }
}

void StreamServer::createSocket()
{
    m_serverSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_serverSocket == -1)
        throw std::system_error(errno, std::generic_category(), "Unix server socket failed");
}

void StreamServer::bindSocket()
{
    unix_socket::bindPrivate(m_serverSocket, m_socketPath);
    m_bound = true;
    if (listen(m_serverSocket, 30) == -1)
        throw std::system_error(errno, std::generic_category(), "Unix listen failed");
}

nx_data StreamServer::receiveMessage(int socket)
{
    auto readExact = [socket](uint8_t* data, size_t length)
    {
        size_t offset = 0;
        while (offset < length)
        {
            ssize_t count = recv(socket, data + offset, length - offset, 0);
            if (count <= 0)
                return false;
            offset += static_cast<size_t>(count);
        }
        return true;
    };

    uint8_t header[4]{};
    if (!readExact(header, sizeof(header)))
        return {};
    uint32_t length = (static_cast<uint32_t>(header[0]) << 24) |
                      (static_cast<uint32_t>(header[1]) << 16) |
                      (static_cast<uint32_t>(header[2]) << 8) | header[3];
    if (length == 0 || length > NEXILIS_BUFFER)
        return {};
    nx_data message(length);
    if (!readExact(message.data(), message.size()))
        return {};
    return message;
}

void StreamServer::handleMessages()
{
    const int clientSocket = accept(m_serverSocket, nullptr, nullptr);
    if (clientSocket == -1)
        return;
    if (!unix_socket::sameUser(clientSocket))
    {
        ::close(clientSocket);
        return;
    }
    std::shared_ptr<Connection> connection;
    try
    {
        connection = std::make_shared<Connection>(clientSocket);
    }
    catch (...)
    {
        ::close(clientSocket);
        throw;
    }
    {
        std::lock_guard lock(*m_mutex);
        if (m_stopped->load())
            return;
        m_client = connection;
    }

    struct CloseConnection
    {
        std::shared_ptr<Connection> connection;
        ~CloseConnection()
        {
            connection->close();
        }
    } closeOnExit{connection};
    uint64_t connectedUserId = 0;
    while (!m_stopped->load())
    {
        nx_data message = receiveMessage(clientSocket);

        if (message.empty())
        {
            break;
        }

        auto handledMessage = getMessageHandler().readMessage("127.0.0.1", message, &getSettings());

        if (!handledMessage || !handledMessage->getUser())
        {
            Log::error("Message from unauthorized client!");
            break;
        }

        auto* user = handledMessage->getUser();
        if (connectedUserId == 0)
            connectedUserId = user->getId();
        if (!user->isUnixStreamSet())
        {
            user->setUnixStreamSend([connection](const nx_data& bytes)
                                    { connection->send(bytes); });
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
                Log::debug("StreamServer: Passed");
            }
        }
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
    {
        std::lock_guard lock(*m_mutex);
        m_client.reset();
    }
}

} // namespace nexilis::server::af_unix

#endif
