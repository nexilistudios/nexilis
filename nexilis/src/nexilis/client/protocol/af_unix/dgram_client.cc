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
    unlink(m_clientSocketPath.c_str());
}

DgramClient::DgramClient(DgramClient&& other)
    : NxClass(std::move(other)),
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
    m_clientSocket = socket(AF_UNIX, SOCK_DGRAM, 0);

    if (m_clientSocket == -1)
    {
        Log::error("Error creating DgramClient socket: ", strerror(errno));
        return;
    }

    // Bind the client to its own socket path so the server can reply to it.
    sockaddr_un clientAddr{};
    memset(&clientAddr, 0, sizeof(clientAddr));
    clientAddr.sun_family = AF_UNIX;
    strcpy(clientAddr.sun_path, m_clientSocketPath.c_str());

    unlink(m_clientSocketPath.c_str());

    auto clientAddress = reinterpret_cast<sockaddr*>(&clientAddr);
    if (bind(m_clientSocket, clientAddress, sizeof(clientAddr)) == -1)
    {
        Log::error("Failed to bind client socket to: ", m_clientSocketPath, " - ", strerror(errno));
        close(m_clientSocket);
        m_clientSocket = -1;
        return;
    }

    // Server address.
    memset(&m_serverAddr, 0, sizeof(m_serverAddr));
    m_serverAddr.sun_family = AF_UNIX;
    strcpy(m_serverAddr.sun_path, m_serverSocketPath.c_str());

    // Set a receive timeout so the receive loop can periodically check the
    // stopped flag and exit cleanly on stop() instead of blocking forever.
    timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; // 100 ms
    if (setsockopt(m_clientSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1)
    {
        Log::error("Error setting DgramClient receive timeout: ", strerror(errno));
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

    std::unique_lock<std::mutex> lock(*m_mutex);

    if (m_clientSocket != -1)
    {
        if (close(m_clientSocket) == -1)
        {
            Log::error("Error closing client socket: ", strerror(errno));
        }
        m_clientSocket = -1;
    }
    lock.unlock();

    if (m_receiveThread.joinable())
    {
        Log::debug("DgramClient closing receiveThread");
        m_receiveThread.join();
    }

    unlink(m_clientSocketPath.c_str());

    Log::debug("DgramClient stopped");
}

void DgramClient::receiveLoop()
{
    while (!m_stopped->load() && m_clientSocket != -1)
    {
        nx_data buffer(NEXILIS_BUFFER);

        ssize_t bytesRead = recv(m_clientSocket, buffer.data(), buffer.size(), 0);

        if (bytesRead > 0)
        {
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
        else
        {
            Log::info("DgramClient: connection closed by peer");
            break;
        }
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

    std::lock_guard<std::mutex> lock(*m_mutex);

    ssize_t sentBytes = sendto(m_clientSocket, message.data(), message.size(), 0,
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

    auto messageCopy = std::make_shared<nx_data>(message);

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
