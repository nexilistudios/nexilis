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

#include <nexilis/client/protocol/af_inet/udp_client.hh>
#include <nexilis/logger/log.hh>
#include <nexilis/nexilis_constants.hh>
#include <nexilis/ports.hh>
#include <nexilis/udp_auth.hh>
#include <nexilis/util.hh>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace nexilis::client::af_inet
{

UDPClient::UDPClient(ClientAPI& clientApi)
    : NxClass("client::af_inet::UDPClient"),
      ClientProtocol(&clientApi),
      m_mutex(std::make_unique<std::mutex>()),
      m_stopped(std::make_unique<std::atomic<bool>>(false))
{
    memset(&m_serverAddr, 0, sizeof(m_serverAddr));
    m_serverAddr.sin_family = AF_INET;
}

UDPClient::~UDPClient()
{
    stop();
}

UDPClient::UDPClient(UDPClient&& other)
    : NxClass(std::move(other)),
      NXProtocol(std::move(other)),
      ClientProtocol(std::move(other)),
      m_clientSocket(std::move(other.m_clientSocket)),
      m_serverAddr(std::move(other.m_serverAddr)),
      m_receiveThread(std::move(other.m_receiveThread)),
      m_mutex(std::move(other.m_mutex)),
      m_stopped(std::move(other.m_stopped)),
      m_pendingAuth(std::move(other.m_pendingAuth))
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

UDPClient& UDPClient::operator=(UDPClient&& other)
{
    if (this != &other)
    {
        static_cast<NxClass&>(*this) = static_cast<NxClass&&>(other);
        static_cast<NXProtocol&>(*this) = static_cast<NXProtocol&&>(other);
        static_cast<ClientProtocol&>(*this) = static_cast<ClientProtocol&&>(other);

        m_clientSocket = std::move(other.m_clientSocket);
        m_serverAddr = std::move(other.m_serverAddr);
        m_receiveThread = std::move(other.m_receiveThread);
        m_mutex = std::move(other.m_mutex);
        m_stopped = std::move(other.m_stopped);
        m_pendingAuth = std::move(other.m_pendingAuth);

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

void UDPClient::createSocket()
{
    m_clientSocket = socket(AF_INET, SOCK_DGRAM, 0);

    if (m_clientSocket == -1)
    {
        Log::error("UDPClient: Error creating socket: ", strerror(errno));
        return;
    }

    // Set a receive timeout so the receive loop can periodically check the
    // stopped flag and exit cleanly on stop() instead of blocking forever.
    timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; // 100 ms
    if (setsockopt(m_clientSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1)
    {
        Log::error("UDPClient: Error setting receive timeout: ", strerror(errno));
    }
}

void UDPClient::start()
{
    if (m_stopped->load())
    {
        Log::debug("UDPClient already stopped, cannot start");
        return;
    }

    uint16_t serverPort = getClientAPI()->getInetUDPServerPortNumber();
    if (serverPort == 0)
    {
        serverPort = Ports::getInetUDPPort();
    }

    const auto& serverAddress = getClientAPI()->getInetUDPServerAddress();
    if (inet_pton(AF_INET, serverAddress.c_str(), &m_serverAddr.sin_addr) <= 0)
    {
        Log::error("UDPClient: Invalid server address: ", serverAddress);
        updateProtocolStatus(ProtocolStatus::error);
        return;
    }
    m_serverAddr.sin_port = htons(serverPort);

    createSocket();

    if (m_clientSocket == -1)
    {
        Log::error("UDPClient: cannot start, socket is not open");
        updateProtocolStatus(ProtocolStatus::error);
        return;
    }

    m_receiveThread = std::thread(&UDPClient::receiveLoop, this);

    try
    {
        ClientProtocol::start(getType());
    }
    catch (const std::exception& e)
    {
        Log::error("Failed to start UDPClient: ", e.what());
        stop();
    }
}

void UDPClient::stop()
{
    if (!m_stopped || m_stopped->exchange(true))
    {
        Log::debug("UDPClient stop already in progress or completed");
        return;
    }

    std::unique_lock<std::mutex> lock(*m_mutex);

    if (m_clientSocket != -1)
    {
        if (close(m_clientSocket) == -1)
        {
            Log::error("UDPClient: Error closing socket: ", strerror(errno));
        }
        m_clientSocket = -1;
    }
    lock.unlock();

    if (m_receiveThread.joinable())
    {
        Log::debug("UDPClient closing receiveThread");
        m_receiveThread.join();
    }

    Log::debug("UDPClient stopped");
}

void UDPClient::receiveLoop()
{
    while (!m_stopped->load() && m_clientSocket != -1)
    {
        nx_data buffer(NEXILIS_BUFFER);

        sockaddr_in srcAddr{};
        socklen_t srcAddrLen = sizeof(srcAddr);
        ssize_t bytesRead = recvfrom(m_clientSocket, buffer.data(), buffer.size(), 0,
                                     reinterpret_cast<sockaddr*>(&srcAddr), &srcAddrLen);

        if (bytesRead > 0)
        {
            if (srcAddr.sin_addr.s_addr != m_serverAddr.sin_addr.s_addr ||
                srcAddr.sin_port != m_serverAddr.sin_port)
                continue;
            buffer.resize(bytesRead);
            Log::info("UDPClient received ", bytesRead, " bytes");

            if (udp_auth::hasPrefix(buffer, udp_auth::challenge))
            {
                if (buffer.size() == 4 + udp_auth::cookie_size)
                {
                    nx_data pending;
                    {
                        std::lock_guard<std::mutex> lock(*m_mutex);
                        pending = m_pendingAuth;
                    }
                    if (!pending.empty())
                    {
                        nx_data cookie(buffer.begin() + 4, buffer.end());
                        auto auth = udp_auth::makeAuthentication(cookie, pending);
                        std::lock_guard<std::mutex> lock(*m_mutex);
                        sendData(reinterpret_cast<const char*>(auth.data()), auth.size());
                    }
                }
                continue;
            }

            auto result = ClientProtocol::getClientAPI()->readMessage(buffer);
            if (result == ReadResult::success)
            {
                Log::debug("UDPClient: Message read successfully");
            }
            else
            {
                Log::error("UDPClient: Received unexpected message");
                Util::debugUint8Vector(buffer);
            }
        }
        else if (bytesRead == -1)
        {
            if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
            {
                Log::error("UDPClient: Error receiving message: ", strerror(errno));
            }
        }
    }
}

bool UDPClient::sendData(const char* data, size_t dataSize)
{
    if (m_clientSocket == -1)
    {
        Log::error("UDPClient::sendData(): socket is not open");
        return false;
    }

    ssize_t sentBytes = sendto(m_clientSocket, data, dataSize, 0,
                               reinterpret_cast<const sockaddr*>(&m_serverAddr),
                               sizeof(m_serverAddr));

    if (sentBytes == -1)
    {
        Log::error("UDPClient: Failed to send message to server: ", strerror(errno));
        return false;
    }

    Log::debug("UDPClient sent ", sentBytes, " bytes to server");
    return true;
}

void UDPClient::sendMessage(const nx_data& message)
{
    if (m_stopped->load())
    {
        Log::error("UDPClient::sendMessage(): client is stopped");
        return;
    }

    if (m_clientSocket == -1)
    {
        Log::error("UDPClient::sendMessage(): socket is not open");
        return;
    }

    nx_data packet;
    auto token = getClientAPI()->getUdpToken();
    if (token.empty())
    {
        packet = udp_auth::makeHello();
    }
    else
    {
        packet = udp_auth::makeSession(message, token);
    }
    if (packet.empty())
        return;
    std::lock_guard<std::mutex> lock(*m_mutex);
    if (token.empty())
        m_pendingAuth = message;
    sendData(reinterpret_cast<const char*>(packet.data()), packet.size());
}

void UDPClient::sendMessage(const nx_data& message, const std::function<void()>& callback)
{
    sendMessageWithCallback(message, callback);
}

std::future<void> UDPClient::sendMessageAsync(const nx_data& message)
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

    auto packet = udp_auth::makeSession(message, getClientAPI()->getUdpToken());
    if (packet.empty())
    {
        promise->set_exception(std::make_exception_ptr(std::runtime_error("UDP session is not authenticated")));
        return future;
    }
    auto messageCopy = std::make_shared<nx_data>(std::move(packet));

    std::thread([this, messageCopy, promise]()
                {
        try
        {
            std::lock_guard<std::mutex> lock(*m_mutex);

            if (m_clientSocket == -1)
            {
                throw std::runtime_error("Socket is not open");
            }

            if (!sendData(reinterpret_cast<const char*>(messageCopy->data()), messageCopy->size()))
            {
                throw std::runtime_error(std::string("Failed to send message: ") + strerror(errno));
            }

            promise->set_value();
        }
        catch (...)
        {
            promise->set_exception(std::current_exception());
        } })
            .detach();

    return future;
}

} // namespace nexilis::client::af_inet
