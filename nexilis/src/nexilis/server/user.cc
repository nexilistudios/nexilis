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

#include <nexilis/server/user.hh>
#include <nexilis/udp_auth.hh>

#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <stdexcept>

namespace nexilis::server
{

User::User(uint64_t id, std::string ip_address)
    : BaseClient(id),
      m_ip_address(ip_address)
{
    std::array<uint8_t, udp_auth::token_size> token{};
    if (RAND_bytes(token.data(), token.size()) != 1)
        throw std::runtime_error("Failed to create UDP session token");
    m_udpToken = udp_auth::toHex(token.data(), token.size());
}

User::User(User&& other) noexcept
    : BaseClient(std::move(other)),
      m_ip_address(std::move(other.m_ip_address)),
      m_udpToken(std::move(other.m_udpToken)),
      m_udpTokenSent(other.m_udpTokenSent.load()),
      m_replayWindow(std::move(other.m_replayWindow)),
      m_username(std::move(other.m_username)),
      m_roomId(std::move(other.m_roomId)),
      m_hasRootAccess(other.m_hasRootAccess.load()),
      m_hasCommonAccess(other.m_hasCommonAccess.load())
{
    {
        std::lock_guard<std::mutex> lock(other.m_shotMutex);
        m_lastShotTime = other.m_lastShotTime;
    }
    {
        std::lock_guard<std::mutex> lock(other.m_broadcastMutex);
        m_lastBroadcastTime = other.m_lastBroadcastTime;
    }
    {
        std::lock_guard<std::mutex> lock(other.m_commandMutex);
        m_commandLimiterInitialized = other.m_commandLimiterInitialized;
        m_commandLastRefill = other.m_commandLastRefill;
        m_commandTokens = other.m_commandTokens;
    }
    {
        std::lock_guard<std::mutex> lock(other.m_sendMutex);
        m_boostTCPSendToClient = std::move(other.m_boostTCPSendToClient);
        m_boostUDPSendToClient = std::move(other.m_boostUDPSendToClient);
        m_unixStreamSendToClient = std::move(other.m_unixStreamSendToClient);
        m_unixDgramSendToClient = std::move(other.m_unixDgramSendToClient);
        m_inetTCPSendToClient = std::move(other.m_inetTCPSendToClient);
        m_inetUDPSendToClient = std::move(other.m_inetUDPSendToClient);
    }
    {
        std::lock_guard<std::mutex> lock(other.m_transportMutex);
        m_streamTransportConnections = other.m_streamTransportConnections;
        m_datagramTransportActive = other.m_datagramTransportActive;
        m_datagramLastActivity = other.m_datagramLastActivity;
        m_pendingHandshakes = other.m_pendingHandshakes;
        m_pendingHandshakeSince = other.m_pendingHandshakeSince;
    }
}

/// Move assignment operator.
User& User::operator=(User&& other) noexcept
{
    if (this != &other)
    {
        m_ip_address = std::move(other.m_ip_address);
        m_udpToken = std::move(other.m_udpToken);
        m_udpTokenSent.store(other.m_udpTokenSent.load());
        m_replayWindow = std::move(other.m_replayWindow);
        m_username = std::move(other.m_username);
        m_roomId = std::move(other.m_roomId);
        {
            std::scoped_lock lock(m_shotMutex, other.m_shotMutex);
            m_lastShotTime = other.m_lastShotTime;
        }
        {
            std::scoped_lock lock(m_broadcastMutex, other.m_broadcastMutex);
            m_lastBroadcastTime = other.m_lastBroadcastTime;
        }
        {
            std::scoped_lock lock(m_commandMutex, other.m_commandMutex);
            m_commandLimiterInitialized = other.m_commandLimiterInitialized;
            m_commandLastRefill = other.m_commandLastRefill;
            m_commandTokens = other.m_commandTokens;
        }
        {
            std::scoped_lock lock(m_sendMutex, other.m_sendMutex);
            m_boostTCPSendToClient = std::move(other.m_boostTCPSendToClient);
            m_boostUDPSendToClient = std::move(other.m_boostUDPSendToClient);
            m_unixStreamSendToClient = std::move(other.m_unixStreamSendToClient);
            m_unixDgramSendToClient = std::move(other.m_unixDgramSendToClient);
            m_inetTCPSendToClient = std::move(other.m_inetTCPSendToClient);
            m_inetUDPSendToClient = std::move(other.m_inetUDPSendToClient);
        }
        {
            std::scoped_lock lock(m_transportMutex, other.m_transportMutex);
            m_streamTransportConnections = other.m_streamTransportConnections;
            m_datagramTransportActive = other.m_datagramTransportActive;
            m_datagramLastActivity = other.m_datagramLastActivity;
            m_pendingHandshakes = other.m_pendingHandshakes;
            m_pendingHandshakeSince = other.m_pendingHandshakeSince;
        }
        m_hasRootAccess.store(other.m_hasRootAccess.load());
        m_hasCommonAccess.store(other.m_hasCommonAccess.load());

        BaseClient::operator=(std::move(other));
    }
    return *this;
}

bool User::acceptMessageId(uint64_t id)
{
    if (id == 0 || !m_replayWindow)
        return false;

    std::lock_guard<std::mutex> lock(m_replayWindow->mutex);
    auto& window = *m_replayWindow;
    if (window.highest == 0)
    {
        window.highest = id;
        window.seen.set(0);
        return true;
    }
    if (id > window.highest)
    {
        uint64_t delta = id - window.highest;
        if (delta >= window.seen.size())
            window.seen.reset();
        else
            window.seen <<= delta;
        window.highest = id;
        window.seen.set(0);
        return true;
    }
    uint64_t behind = window.highest - id;
    if (behind >= window.seen.size() || window.seen.test(behind))
        return false;
    window.seen.set(behind);
    return true;
}

bool User::acceptCommand(std::chrono::steady_clock::time_point now,
                         float ratePerSecond,
                         float burstCapacity)
{
    if (!(ratePerSecond > 0.0f))
        return true;

    const double capacity = std::max(1.0, static_cast<double>(burstCapacity));
    const double rate = static_cast<double>(ratePerSecond);

    std::lock_guard<std::mutex> lock(m_commandMutex);
    if (!m_commandLimiterInitialized)
    {
        m_commandTokens = capacity;
        m_commandLastRefill = now;
        m_commandLimiterInitialized = true;
    }
    else if (now > m_commandLastRefill)
    {
        const double elapsed = std::chrono::duration<double>(now - m_commandLastRefill).count();
        m_commandTokens = std::min(capacity, m_commandTokens + elapsed * rate);
        m_commandLastRefill = now;
    }

    if (m_commandTokens < 1.0)
        return false;

    m_commandTokens -= 1.0;
    return true;
}

bool User::boostTCPSend(const nx_data& data)
{
    std::function<void(nx_data)> send;
    {
        std::lock_guard<std::mutex> lock(m_sendMutex);
        send = m_boostTCPSendToClient;
    }
    if (send)
    {
        send(data);
        return true;
    }
    return false;
}

bool User::boostUDPSend(const nx_data& data)
{
    std::function<void(nx_data)> send;
    {
        std::lock_guard<std::mutex> lock(m_sendMutex);
        send = m_boostUDPSendToClient;
    }
    if (send)
    {
        send(data);
        return true;
    }
    return false;
}

bool User::unixStreamSend(const nx_data& data)
{
    std::function<void(nx_data)> send;
    {
        std::lock_guard<std::mutex> lock(m_sendMutex);
        send = m_unixStreamSendToClient;
    }
    if (send)
    {
        send(data);
        return true;
    }
    return false;
}

bool User::unixDgramSend(const nx_data& data)
{
    std::function<void(nx_data)> send;
    {
        std::lock_guard<std::mutex> lock(m_sendMutex);
        send = m_unixDgramSendToClient;
    }
    if (send)
    {
        send(data);
        return true;
    }
    return false;
}

bool User::inetTCPSend(const nx_data& data)
{
    std::function<void(nx_data)> send;
    {
        std::lock_guard<std::mutex> lock(m_sendMutex);
        send = m_inetTCPSendToClient;
    }
    if (send)
    {
        send(data);
        return true;
    }
    return false;
}

bool User::inetUDPSend(const nx_data& data)
{
    std::function<void(nx_data)> send;
    {
        std::lock_guard<std::mutex> lock(m_sendMutex);
        send = m_inetUDPSendToClient;
    }
    if (send)
    {
        send(data);
        return true;
    }
    return false;
}

namespace
{
constexpr std::size_t transportIndex(User::Transport transport)
{
    return static_cast<std::size_t>(transport);
}

bool hasPresenceLocked(
        const std::array<std::size_t, User::transportCount>& streamConnections,
        const std::array<bool, User::transportCount>& datagramActive,
        std::size_t pendingHandshakes)
{
    if (pendingHandshakes != 0)
        return true;
    for (std::size_t i = 0; i < User::transportCount; ++i)
    {
        if (streamConnections[i] != 0 || datagramActive[i])
            return true;
    }
    return false;
}
} // namespace

void User::registerStreamTransport(Transport transport)
{
    const auto index = transportIndex(transport);
    if (index >= transportCount)
        return;

    std::lock_guard<std::mutex> lock(m_transportMutex);
    ++m_streamTransportConnections[index];
}

User::TransportStatus User::endStreamTransport(Transport transport)
{
    const auto index = transportIndex(transport);
    TransportStatus status;
    if (index >= transportCount)
        return status;

    std::lock_guard<std::mutex> lock(m_transportMutex);
    if (m_streamTransportConnections[index] != 0)
        --m_streamTransportConnections[index];
    status.anyActive = hasPresenceLocked(
            m_streamTransportConnections, m_datagramTransportActive, m_pendingHandshakes);
    status.transportActive = m_streamTransportConnections[index] != 0;
    return status;
}

void User::touchDatagramTransport(Transport transport,
                                  std::chrono::steady_clock::time_point now)
{
    const auto index = transportIndex(transport);
    if (index >= transportCount)
        return;

    std::lock_guard<std::mutex> lock(m_transportMutex);
    m_datagramTransportActive[index] = true;
    m_datagramLastActivity[index] = now;
}

User::TransportStatus User::endDatagramTransport(Transport transport)
{
    const auto index = transportIndex(transport);
    TransportStatus status;
    if (index >= transportCount)
        return status;

    std::lock_guard<std::mutex> lock(m_transportMutex);
    m_datagramTransportActive[index] = false;
    m_datagramLastActivity[index] = {};
    status.anyActive = hasPresenceLocked(
            m_streamTransportConnections, m_datagramTransportActive, m_pendingHandshakes);
    status.transportActive = false;
    return status;
}

void User::beginPendingHandshake(std::chrono::steady_clock::time_point now)
{
    std::lock_guard<std::mutex> lock(m_transportMutex);
    if (m_pendingHandshakes == 0)
        m_pendingHandshakeSince = now;
    ++m_pendingHandshakes;
}

User::TransportStatus User::endPendingHandshake()
{
    TransportStatus status;
    std::lock_guard<std::mutex> lock(m_transportMutex);
    if (m_pendingHandshakes != 0)
        --m_pendingHandshakes;
    if (m_pendingHandshakes == 0)
        m_pendingHandshakeSince = {};

    status.anyActive = hasPresenceLocked(
            m_streamTransportConnections, m_datagramTransportActive, m_pendingHandshakes);
    const auto boostTcpIndex = transportIndex(Transport::boostTcp);
    status.transportActive = m_streamTransportConnections[boostTcpIndex] != 0 ||
                             m_pendingHandshakes != 0;
    return status;
}

std::optional<User::TransportStatus> User::expirePendingHandshakes(
        std::chrono::steady_clock::time_point now,
        std::chrono::milliseconds timeout)
{
    if (timeout <= std::chrono::milliseconds::zero())
        return std::nullopt;

    std::lock_guard<std::mutex> lock(m_transportMutex);
    if (m_pendingHandshakes == 0 || now - m_pendingHandshakeSince < timeout)
        return std::nullopt;

    m_pendingHandshakes = 0;
    m_pendingHandshakeSince = {};

    TransportStatus status;
    status.anyActive = hasPresenceLocked(
            m_streamTransportConnections, m_datagramTransportActive, m_pendingHandshakes);
    const auto boostTcpIndex = transportIndex(Transport::boostTcp);
    status.transportActive = m_streamTransportConnections[boostTcpIndex] != 0;
    return status;
}

std::optional<User::TransportStatus> User::cancelPendingHandshakes()
{
    std::lock_guard<std::mutex> lock(m_transportMutex);
    if (m_pendingHandshakes == 0)
        return std::nullopt;

    m_pendingHandshakes = 0;
    m_pendingHandshakeSince = {};

    TransportStatus status;
    status.anyActive = hasPresenceLocked(
            m_streamTransportConnections, m_datagramTransportActive, m_pendingHandshakes);
    const auto boostTcpIndex = transportIndex(Transport::boostTcp);
    status.transportActive = m_streamTransportConnections[boostTcpIndex] != 0;
    return status;
}

std::optional<User::TransportStatus> User::expireDatagramTransport(
        Transport transport,
        std::chrono::steady_clock::time_point now,
        std::chrono::milliseconds timeout)
{
    if (timeout <= std::chrono::milliseconds::zero())
        return std::nullopt;

    const auto index = transportIndex(transport);
    if (index >= transportCount)
        return std::nullopt;

    std::lock_guard<std::mutex> lock(m_transportMutex);
    if (!m_datagramTransportActive[index] ||
        now - m_datagramLastActivity[index] < timeout)
        return std::nullopt;

    m_datagramTransportActive[index] = false;
    m_datagramLastActivity[index] = {};

    TransportStatus status;
    status.anyActive = hasPresenceLocked(
            m_streamTransportConnections, m_datagramTransportActive, m_pendingHandshakes);
    status.transportActive = false;
    return status;
}

bool User::hasTransportPresence() const
{
    std::lock_guard<std::mutex> lock(m_transportMutex);
    return hasPresenceLocked(
            m_streamTransportConnections, m_datagramTransportActive, m_pendingHandshakes);
}

bool User::hasTransport(Transport transport) const
{
    const auto index = transportIndex(transport);
    if (index >= transportCount)
        return false;

    std::lock_guard<std::mutex> lock(m_transportMutex);
    return m_streamTransportConnections[index] != 0 ||
           m_datagramTransportActive[index];
}

void User::clearSendCallback(Transport transport)
{
    std::lock_guard<std::mutex> lock(m_sendMutex);
    switch (transport)
    {
        case Transport::boostTcp:
            m_boostTCPSendToClient = nullptr;
            break;
        case Transport::boostUdp:
            m_boostUDPSendToClient = nullptr;
            break;
        case Transport::unixStream:
            m_unixStreamSendToClient = nullptr;
            break;
        case Transport::unixDgram:
            m_unixDgramSendToClient = nullptr;
            break;
        case Transport::inetTcp:
            m_inetTCPSendToClient = nullptr;
            break;
        case Transport::inetUdp:
            m_inetUDPSendToClient = nullptr;
            break;
    }
}

} // namespace nexilis::server
