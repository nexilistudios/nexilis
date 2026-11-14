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
#include <nexilis/unix_socket.hh>

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <system_error>

namespace nexilis::server::af_unix
{

struct DgramServer::SocketState
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

    void send(const sockaddr_un& address, const nx_data& bytes)
    {
        std::lock_guard lock(mutex);
        if (!acceptingSends || fd == -1)
            throw std::runtime_error("Unix datagram socket is closed");
        const auto sent = sendto(fd, bytes.data(), bytes.size(), MSG_DONTWAIT | MSG_NOSIGNAL,
                                 reinterpret_cast<const sockaddr*>(&address), sizeof(address));
        if (sent == -1)
            throw std::system_error(errno, std::generic_category(), "Unix datagram send failed");
    }

    // The descriptor stays unchanged while the receive worker runs. stop()
    // disables callbacks first, joins that worker, then closes under the mutex.
    int fd = -1;
    bool acceptingSends = true;
    std::mutex mutex;
};

DgramServer::DgramServer(const ServerConfig& settings, const std::string& socketPath)
    : ServerProtocol(settings),
      m_socketPath(socketPath),
      m_socketState(std::make_shared<SocketState>()),
      m_stopped(std::make_unique<std::atomic<bool>>(false))
{
    unix_socket::address(m_socketPath);
    createSocket();
    try
    {
        bindSocket();
    }
    catch (...)
    {
        m_socketState->close();
        throw;
    }
}

DgramServer::~DgramServer()
{
    stop();
    if (m_bound)
        unlink(m_socketPath.c_str());
}

DgramServer& DgramServer::movable(DgramServer& protocol)
{
    if (protocol.m_receiveThread.joinable())
        throw std::logic_error("Stop a Unix datagram protocol before moving it");
    return protocol;
}

DgramServer::DgramServer(DgramServer&& other)
    : NXProtocol(std::move(movable(other))),
      ServerProtocol(std::move(other)),
      m_socketPath(std::move(other.m_socketPath)),
      m_socketState(std::move(other.m_socketState)),
      m_receiveThread(std::move(other.m_receiveThread)),
      m_stopped(std::move(other.m_stopped))
{
    m_bound = other.m_bound;
    other.m_bound = false;
    if (!m_stopped)
    {
        m_stopped = std::make_unique<std::atomic<bool>>(false);
    }
}

DgramServer& DgramServer::operator=(DgramServer&& other)
{
    if (this != &other)
    {
        movable(other);
        stop();
        if (m_bound)
            unlink(m_socketPath.c_str());
        static_cast<NXProtocol&>(*this) = static_cast<NXProtocol&&>(other);
        static_cast<ServerProtocol&>(*this) = static_cast<ServerProtocol&&>(other);

        m_socketPath = std::move(other.m_socketPath);
        m_socketState = std::move(other.m_socketState);
        m_receiveThread = std::move(other.m_receiveThread);
        m_stopped = std::move(other.m_stopped);

        m_bound = other.m_bound;
        other.m_bound = false;
        if (!m_stopped)
        {
            m_stopped = std::make_unique<std::atomic<bool>>(false);
        }
    }
    return *this;
}

void DgramServer::start()
{
    if (!m_stopped || m_stopped->load() || m_receiveThread.joinable())
    {
        Log::debug("DgramServer already stopped, cannot start");
        return;
    }

    if (m_socketState->fd == -1)
    {
        Log::error("DgramServer: cannot start, socket is not open");
        return;
    }

    m_receiveThread = std::thread(&DgramServer::receiveFromClients, this);
    Log::info("DgramServer started at: ", m_socketPath);
}

void DgramServer::stop()
{
    if (!m_stopped || !m_socketState)
        return;
    m_stopped->store(true);
    m_socketState->disableSends();
    if (m_receiveThread.joinable() && m_receiveThread.get_id() == std::this_thread::get_id())
        return;
    if (m_receiveThread.joinable())
        m_receiveThread.join();
    cleanupDatagramTransports(User::Transport::unixDgram, *this);
    m_socketState->close();

    Log::debug("DgramServer stopped");
}

void DgramServer::createSocket()
{
    m_socketState->fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (m_socketState->fd == -1)
        throw std::system_error(errno, std::generic_category(), "Unix server socket failed");
    const int enabled = 1;
    timeval timeout{};
    timeout.tv_usec = 100000;
    if (setsockopt(m_socketState->fd, SOL_SOCKET, SO_PASSCRED, &enabled, sizeof(enabled)) == -1 ||
        setsockopt(m_socketState->fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1)
    {
        const auto error = errno;
        m_socketState->close();
        throw std::system_error(error, std::generic_category(), "Unix server socket setup failed");
    }
}

void DgramServer::bindSocket()
{
    unix_socket::bindPrivate(m_socketState->fd, m_socketPath);
    m_bound = true;
}

void DgramServer::receiveFromClients()
{
    while (!m_stopped->load() && m_socketState->fd != -1)
    {
        expireIdleDatagramTransports(User::Transport::unixDgram, *this);
        sockaddr_un clientAddress{};
        nx_data buffer(NEXILIS_BUFFER);
        iovec vector{buffer.data(), buffer.size()};
        alignas(cmsghdr) char control[CMSG_SPACE(sizeof(ucred))]{};
        msghdr message{};
        message.msg_name = &clientAddress;
        message.msg_namelen = sizeof(clientAddress);
        message.msg_iov = &vector;
        message.msg_iovlen = 1;
        message.msg_control = control;
        message.msg_controllen = sizeof(control);
        const ssize_t bytesReceived = recvmsg(m_socketState->fd, &message, 0);

        if (bytesReceived <= 0)
        {
            if (bytesReceived == -1 && errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
            {
                Log::error("Error receiving message: ", strerror(errno));
            }
            continue;
        }

        if (!unix_socket::sameUser(message) || clientAddress.sun_path[0] == '\0' ||
            !std::memchr(clientAddress.sun_path, '\0', sizeof(clientAddress.sun_path)))
            continue;

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
        touchDatagramTransport(*user, User::Transport::unixDgram);
        if (!user->isUnixDgramSet())
        {
            auto senderAddress = clientAddress;
            user->setUnixDgramSend([socket = m_socketState, senderAddress](const nx_data& bytes)
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
                Log::debug("DgramServer: Passed");
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
