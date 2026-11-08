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

#include <nexilis/client/protocol/nxboost/udp_client.hh>
#include <nexilis/logger/log.hh>
#include <nexilis/nexilis_constants.hh>
#include <nexilis/ports.hh>
#include <nexilis/udp_auth.hh>

#include <boost/asio/ip/udp.hpp>

#include <mutex>

namespace nexilis::client::nxboost
{

UDPClient::UDPClient(ClientAPI& clientApi)
    : NxClass("client::nxboost::UDPClient"),
      ClientProtocol(&clientApi),
      m_stopped(std::make_unique<std::atomic<bool>>(false)),
      m_ioContext(std::make_unique<boost::asio::io_context>()),
      m_mutex(std::make_unique<std::mutex>()),
      m_socket(*m_ioContext),
      m_receiveBuffer(NEXILIS_BUFFER)
{
}

UDPClient::~UDPClient()
{
    stop();
}

UDPClient::UDPClient(UDPClient&& other)
    : NxClass(std::move(other)),
      NXProtocol(std::move(other)),
      ClientProtocol(std::move(other)),
      m_stopped(std::move(other.m_stopped) ? std::move(other.m_stopped) : std::make_unique<std::atomic<bool>>(false)),
      m_ioContext(std::move(other.m_ioContext)),
      m_ioContextThread(std::move(other.m_ioContextThread)),
      m_receiveMessageThread(std::move(other.m_receiveMessageThread)),
      m_mutex(std::move(other.m_mutex)),
      m_socket(std::move(other.m_socket)),
      m_receiveBuffer(std::move(other.m_receiveBuffer)),
      m_remoteEndpoint(std::move(other.m_remoteEndpoint)),
      m_sendEndpoint(std::move(other.m_sendEndpoint)),
      m_pendingAuth(std::move(other.m_pendingAuth))
{
    other.m_ioContext = nullptr;
    other.m_mutex = nullptr;
}

UDPClient& UDPClient::operator=(UDPClient&& other)
{
    if (this != &other)
    {
        static_cast<NxClass&>(*this) = static_cast<NxClass&&>(other);
        static_cast<ClientProtocol&>(*this) = static_cast<ClientProtocol&&>(other);
        static_cast<NXProtocol&>(*this) = static_cast<NXProtocol&&>(other);

        m_stopped = std::move(other.m_stopped);
        if (!m_stopped)
        {
            m_stopped = std::make_unique<std::atomic<bool>>(false);
        }
        m_ioContext = std::move(other.m_ioContext);
        m_ioContextThread = std::move(other.m_ioContextThread);
        m_receiveMessageThread = std::move(other.m_receiveMessageThread);
        m_mutex = std::move(other.m_mutex);
        m_socket = std::move(other.m_socket);
        m_receiveBuffer = std::move(other.m_receiveBuffer);
        m_remoteEndpoint = std::move(other.m_remoteEndpoint);
        m_sendEndpoint = std::move(other.m_sendEndpoint);
        m_pendingAuth = std::move(other.m_pendingAuth);

        other.m_ioContext = nullptr;
        other.m_mutex = nullptr;
    }
    return *this;
}

void UDPClient::start()
{
    if (m_stopped->load())
    {
        Log::debug("BoostUDPClient already stopped, cannot start");
        return;
    }

    auto port = getClientAPI()->getBoostUDPServerPortNumber();
    if (port == 0)
    {
        port = Ports::getBoostUDPPort();
    }

    try
    {
        boost::asio::ip::udp::resolver resolver(*m_ioContext);
        auto endpoints = resolver.resolve(boost::asio::ip::udp::v4(),
                                          getClientAPI()->getBoostUDPServerAddress(),
                                          std::to_string(port));
        m_remoteEndpoint = *endpoints.begin();

        m_sendEndpoint = m_remoteEndpoint;
        m_ioContextThread = std::thread([this]()
                                        { m_ioContext->run(); });
        m_socket.open(boost::asio::ip::udp::v4());
        // Windows rejects receive_from on an unbound socket (WSAEINVAL).
        m_socket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0));
        m_receiveMessageThread = std::thread(&UDPClient::receiveLoop, this);

        ClientProtocol::start(getType());
    }
    catch (const std::exception& e)
    {
        Log::error("Failed to start BoostUDPClient: ", e.what());
        stop();
    }
}

void UDPClient::stop()
{
    if (!m_stopped || m_stopped->exchange(true))
    {
        Log::debug("BoostUDPClient stop already in progress or completed");
        return;
    }

    boost::system::error_code ec;
    std::unique_lock<std::mutex> lock(*m_mutex);
    if (m_socket.is_open())
    {
        m_socket.shutdown(boost::asio::ip::udp::socket::shutdown_both, ec);

        if (m_socket.cancel(ec))
        {
            Log::error("Error cancelling socket: ", ec.message());
        }

        if (m_socket.close(ec))
        {
            Log::error("Error closing socket: ", ec.message());
        }
    }
    lock.unlock();

    if (m_ioContext)
    {
        Log::debug("BoostUDPClient io context stopped");
        m_ioContext->stop();
    }

    if (m_ioContextThread.joinable())
    {
        Log::debug("BoostUDPClient io context thread stopped");
        m_ioContextThread.join();
    }

    if (m_receiveMessageThread.joinable())
    {
        Log::debug("BoostUDPClient receive message thread stopped");
        m_receiveMessageThread.join();
    }

    Log::debug("BoostUDPClient stopped");
}

void UDPClient::receiveLoop()
{
    while (m_socket.is_open())
    {
        boost::system::error_code recvEc;
        boost::asio::ip::udp::endpoint senderEndpoint;
        std::size_t receivedBytes = m_socket.receive_from(boost::asio::buffer(m_receiveBuffer), senderEndpoint, 0, recvEc);

        if (recvEc)
        {
            if (m_stopped->load() || recvEc == boost::asio::error::operation_aborted ||
                recvEc == boost::asio::error::bad_descriptor)
            {
                break;
            }
            Log::error("BoostUDPClient receive failed: ", recvEc.message());
            continue;
        }

        if (m_stopped->load())
        {
            break;
        }

        if (receivedBytes == 0 || senderEndpoint != m_sendEndpoint)
        {
            continue;
        }

        Log::info("Received bytes: ", receivedBytes);

        auto byteVector = Util::convertToByteVector(m_receiveBuffer.data(), receivedBytes);
        if (udp_auth::hasPrefix(byteVector, udp_auth::challenge))
        {
            if (byteVector.size() == 4 + udp_auth::cookie_size)
            {
                nx_data pending;
                {
                    std::lock_guard<std::mutex> lock(*m_mutex);
                    pending = m_pendingAuth;
                }
                if (!pending.empty())
                {
                    nx_data cookie(byteVector.begin() + 4, byteVector.end());
                    auto auth = udp_auth::makeAuthentication(cookie, pending);
                    m_socket.send_to(boost::asio::buffer(auth), m_sendEndpoint);
                }
            }
            continue;
        }
        auto result = ClientProtocol::getClientAPI()->readMessage(byteVector);
        if (result == ReadResult::success)
        {
            Log::debug("BoostUDPClient: Message read successfully");
        }
        else
        {
            Log::error("Received unexpected message (", byteVector.size(), " bytes)");
        }
    }
}

void UDPClient::sendMessage(const nx_data& payload)
{
    if (m_stopped->load())
    {
        Log::error("UDPClient::send(): client is stopped");
        return;
    }

    if (!m_socket.is_open())
    {
        Log::error("UDPClient::send(): socket is not open");
        return;
    }

    try
    {
        nx_data packet;
        auto token = getClientAPI()->getUdpToken();
        if (token.empty())
        {
            std::lock_guard<std::mutex> lock(*m_mutex);
            m_pendingAuth = payload;
            packet = udp_auth::makeHello();
        }
        else
        {
            packet = udp_auth::makeSession(payload, token);
        }
        if (packet.empty())
            return;
        size_t bytes_sent = m_socket.send_to(boost::asio::buffer(packet), m_sendEndpoint);
        Log::debug("Sent ", bytes_sent, " bytes to server");
    }
    catch (const boost::system::system_error& e)
    {
        Log::error("Failed to send message: ", e.what());
    }
    catch (const std::exception& e)
    {
        Log::error("Exception in sendMessage: ", e.what());
    }
}

void UDPClient::sendMessage(const nx_data& payload, const std::function<void()>& callback)
{
    sendMessageWithCallback(payload, callback);
}

std::future<void> UDPClient::sendMessageAsync(const nx_data& message)
{
    std::promise<void> promise;
    auto future = promise.get_future();
    try
    {
        std::lock_guard lock(*m_mutex);
        if (m_stopped->load() || !m_socket.is_open())
            throw std::runtime_error("UDP socket is not open");
        auto packet = udp_auth::makeSession(message, getClientAPI()->getUdpToken());
        if (packet.empty())
            throw std::runtime_error("UDP session is not authenticated");
        m_socket.send_to(boost::asio::buffer(packet), m_sendEndpoint);
        promise.set_value();
    }
    catch (...)
    {
        promise.set_exception(std::current_exception());
    }
    return future;
}

} // namespace nexilis::client::nxboost
