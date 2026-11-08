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

#include <nexilis/client/protocol/af_unix/dgram_client.hh>
#include <nexilis/logger/log.hh>
#include <nexilis/nexilis_constants.hh>
#include <nexilis/unix_socket.hh>

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <sstream>

namespace nexilis::client::af_unix
{

namespace
{

/// Process-local counter used to make each client socket path unique so
/// multiple clients in the same process never collide on the filesystem.
std::atomic<uint64_t>& dgramClientCounter()
{
    static std::atomic<uint64_t> counter{0};
    return counter;
}

/// Build a unique bind path for a client connecting to `serverSocketPath`.
/// The client must bind its own socket so the server can reply to it.
std::string makeClientSocketPath(const std::string& serverSocketPath)
{
    auto id = dgramClientCounter().fetch_add(1);
    std::ostringstream oss;
    oss << serverSocketPath << "_client_" << getpid() << "_" << id;
    return oss.str();
}

} // namespace

DgramClient::DgramClient(ClientAPI& clientApi)
    : NxClass("client::af_unix::DgramClient"),
      ClientProtocol(&clientApi),
      m_serverSocketPath(clientApi.getUnixDgramPath()),
      m_clientSocketPath(makeClientSocketPath(m_serverSocketPath)),
      m_mutex(std::make_unique<std::mutex>()),
      m_stopped(std::make_unique<std::atomic<bool>>(false))
{
}

DgramClient::~DgramClient()
{
    stop();
    if (m_bound)
        unlink(m_clientSocketPath.c_str());
}

DgramClient& DgramClient::movable(DgramClient& protocol)
{
    if (protocol.m_receiveThread.joinable())
        throw std::logic_error("Stop a Unix datagram protocol before moving it");
    return protocol;
}

DgramClient::DgramClient(DgramClient&& other)
    : NxClass(std::move(movable(other))),
      NXProtocol(std::move(other)),
      ClientProtocol(std::move(other)),
      m_serverSocketPath(std::move(other.m_serverSocketPath)),
      m_clientSocketPath(std::move(other.m_clientSocketPath)),
      m_clientSocket(std::move(other.m_clientSocket)),
      m_serverAddr(std::move(other.m_serverAddr)),
      m_receiveThread(std::move(other.m_receiveThread)),
      m_mutex(std::move(other.m_mutex)),
      m_stopped(std::move(other.m_stopped))
{
    m_bound = other.m_bound;
    other.m_bound = false;
    other.m_clientSocket = -1;
    if (!m_mutex)
    {
        m_mutex = std::make_unique<std::mutex>();
    }
    if (!m_stopped)
    {
        m_stopped = std::make_unique<std::atomic<bool>>(false);
    }
}

DgramClient& DgramClient::operator=(DgramClient&& other)
{
    if (this != &other)
    {
        movable(other);
        stop();
        if (m_bound)
            unlink(m_clientSocketPath.c_str());
        static_cast<NxClass&>(*this) = static_cast<NxClass&&>(other);
        static_cast<NXProtocol&>(*this) = static_cast<NXProtocol&&>(other);
        static_cast<ClientProtocol&>(*this) = static_cast<ClientProtocol&&>(other);

        m_serverSocketPath = std::move(other.m_serverSocketPath);
        m_clientSocketPath = std::move(other.m_clientSocketPath);
        m_clientSocket = std::move(other.m_clientSocket);
        m_serverAddr = std::move(other.m_serverAddr);
        m_receiveThread = std::move(other.m_receiveThread);
        m_mutex = std::move(other.m_mutex);
        m_stopped = std::move(other.m_stopped);

        m_bound = other.m_bound;
        other.m_bound = false;
        other.m_clientSocket = -1;
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

void DgramClient::createSocket()
{
    // Validate both the configured path and the longer generated reply path
    // before creating or binding any descriptor.
    m_serverAddr = unix_socket::address(m_serverSocketPath);
    unix_socket::address(m_clientSocketPath);
    m_clientSocket = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (m_clientSocket == -1)
        throw std::system_error(errno, std::generic_category(), "Unix client socket failed");
    try
    {
        const int enabled = 1;
        if (setsockopt(m_clientSocket, SOL_SOCKET, SO_PASSCRED, &enabled, sizeof(enabled)) == -1)
            throw std::system_error(errno, std::generic_category(), "Unix client credentials failed");
        unix_socket::bindPrivate(m_clientSocket, m_clientSocketPath);
        m_bound = true;
        timeval timeout{};
        timeout.tv_usec = 100000;
        if (setsockopt(m_clientSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1)
            throw std::system_error(errno, std::generic_category(), "Unix client timeout failed");
    }
    catch (...)
    {
        close(m_clientSocket);
        m_clientSocket = -1;
        if (m_bound)
            unlink(m_clientSocketPath.c_str());
        m_bound = false;
        throw;
    }
}

void DgramClient::start()
{
    if (m_stopped->load())
    {
        Log::debug("DgramClient already stopped, cannot start");
        return;
    }

    createSocket();

    if (m_clientSocket == -1)
    {
        Log::error("DgramClient: cannot start, socket is not open");
        updateProtocolStatus(ProtocolStatus::error);
        return;
    }

    m_receiveThread = std::thread(&DgramClient::receiveLoop, this);

    try
    {
        ClientProtocol::start(getType());
    }
    catch (const std::exception& e)
    {
        Log::error("Failed to start DgramClient: ", e.what());
        stop();
    }
}

void DgramClient::stop()
{
    if (!m_stopped || m_stopped->exchange(true))
    {
        Log::debug("DgramClient stop already in progress or completed");
        return;
    }

    if (m_receiveThread.joinable())
        m_receiveThread.join();
    std::lock_guard lock(*m_mutex);
    if (m_clientSocket != -1)
    {
        close(m_clientSocket);
        m_clientSocket = -1;
    }
    if (m_bound)
        unlink(m_clientSocketPath.c_str());
    m_bound = false;

    Log::debug("DgramClient stopped");
}

void DgramClient::receiveLoop()
{
    while (!m_stopped->load() && m_clientSocket != -1)
    {
        nx_data buffer(NEXILIS_BUFFER);

        iovec vector{buffer.data(), buffer.size()};
        alignas(cmsghdr) char control[CMSG_SPACE(sizeof(ucred))]{};
        msghdr message{};
        message.msg_iov = &vector;
        message.msg_iovlen = 1;
        message.msg_control = control;
        message.msg_controllen = sizeof(control);
        const ssize_t bytesRead = recvmsg(m_clientSocket, &message, 0);

        if (bytesRead > 0)
        {
            if (!unix_socket::sameUser(message))
                continue;
            buffer.resize(bytesRead);
            Log::info("DgramClient received ", bytesRead, " bytes");

            auto result = ClientProtocol::getClientAPI()->readMessage(buffer);
            if (result == ReadResult::success)
            {
                Log::debug("DgramClient: Message read successfully");
            }
            else
            {
                Log::error("DgramClient: Received unexpected message");
                Util::debugUint8Vector(buffer);
            }
        }
        else if (bytesRead == -1)
        {
            if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
            {
                Log::error("Error receiving message: ", strerror(errno));
            }
        }
        // A zero-length datagram is not a connection close.
    }
}

void DgramClient::sendMessage(const nx_data& message)
{
    if (m_stopped->load())
    {
        Log::error("DgramClient::sendMessage(): client is stopped");
        return;
    }

    if (m_clientSocket == -1)
    {
        Log::error("DgramClient::sendMessage(): socket is not open");
        return;
    }

    auto request = authenticate(message);
    if (request.empty())
        return;
    std::lock_guard<std::mutex> lock(*m_mutex);

    ssize_t sentBytes = sendto(m_clientSocket, request.data(), request.size(), 0,
                               reinterpret_cast<const sockaddr*>(&m_serverAddr),
                               sizeof(m_serverAddr));

    if (sentBytes == -1)
    {
        Log::error("Failed to send message to server: ", strerror(errno));
    }
    else
    {
        Log::debug("DgramClient sent ", sentBytes, " bytes to server");
    }
}

void DgramClient::sendMessage(const nx_data& message, const std::function<void()>& callback)
{
    sendMessageWithCallback(message, callback);
}

std::future<void> DgramClient::sendMessageAsync(const nx_data& message)
{
    if (m_stopped->load())
    {
        std::promise<void> promise;
        promise.set_exception(std::make_exception_ptr(std::runtime_error("Client is stopped")));
        return promise.get_future();
    }

    if (m_clientSocket == -1)
    {
        std::promise<void> promise;
        promise.set_exception(std::make_exception_ptr(std::runtime_error("Socket is not open")));
        return promise.get_future();
    }

    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();

    auto request = authenticate(message);
    if (request.empty())
    {
        promise->set_exception(std::make_exception_ptr(std::runtime_error("Cannot authenticate request")));
        return future;
    }
    auto messageCopy = std::make_shared<nx_data>(std::move(request));

    std::thread([this, messageCopy, promise]()
                {
        try
        {
            std::lock_guard<std::mutex> lock(*m_mutex);

            if (m_clientSocket == -1)
            {
                throw std::runtime_error("Socket is not open");
            }

            ssize_t sentBytes = sendto(m_clientSocket, messageCopy->data(), messageCopy->size(), 0,
                                       reinterpret_cast<const sockaddr*>(&m_serverAddr),
                                       sizeof(m_serverAddr));

            if (sentBytes == -1)
            {
                throw std::runtime_error(std::string("Failed to send message: ") + strerror(errno));
            }

            Log::debug("DgramClient async sent ", sentBytes, " bytes to server");
            promise->set_value();
        }
        catch (...)
        {
            promise->set_exception(std::current_exception());
        } })
            .detach();

    return future;
}

} // namespace nexilis::client::af_unix

#endif // __linux__
