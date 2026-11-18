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

#ifndef NEXILIS_SERVER_CONFIG_HH
#define NEXILIS_SERVER_CONFIG_HH

#include <nexilis/auth_config.hh>
#include <nexilis/server/gameplay_limits.hh>
#include <nexilis/server/room_limits.hh>

#include <cassert>
#include <chrono>
#include <cstddef>
#include <string>

namespace nexilis::server
{

class ServerConfig : public AuthConfig
{
public:
    ServerConfig() = default;

    using AuthConfig::AuthConfig;

    // Normal password.
    void setPassphrase(const std::string& password)
    {
        setPassword(password);
    }
    bool isPassphrase(const std::string& password)
    {
        return AuthConfig::isPassword(password);
    }
    bool hasPassphrase()
    {
        return AuthConfig::hasPassword();
    }
    const std::string& getPassphrase() const
    {
        return getPassword();
    }

    // Root password.
    void setRootPassword(const std::string& password);
    bool isRootPassword(const std::string& password);
    bool hasRootPassword();
    const std::string& getRootPassword() const
    {
        return m_rootPassword;
    }

    // Tickrate
    void setTickrate(float tickrate);
    float getTickrate() const
    {
        return m_tickrate;
    }

    /// Maximum sustained commands per second accepted from one client.
    /// Commands over the limit are dropped before dispatch. The limiter
    /// allows a burst of up to one second's worth of commands. A
    /// non-positive value disables the limit.
    void setMaxCommandRate(float maxCommandRate)
    {
        m_maxCommandRate = maxCommandRate;
    }
    float getMaxCommandRate() const
    {
        return m_maxCommandRate;
    }

    /// Maximum simultaneous Boost TCP workers waiting for initial authentication
    /// or a session switch proof. Zero refuses new pending connections.
    /// Configure before starting the server.
    void setMaxPendingBoostTcpConnections(std::size_t maximum)
    {
        m_maxPendingBoostTcpConnections = maximum;
    }
    std::size_t getMaxPendingBoostTcpConnections() const
    {
        return m_maxPendingBoostTcpConnections;
    }

    /// Maximum total Boost TCP connection workers, including active sessions.
    /// Zero refuses all new connections. Configure before starting the server.
    void setMaxBoostTcpConnections(std::size_t maximum)
    {
        m_maxBoostTcpConnections = maximum;
    }
    std::size_t getMaxBoostTcpConnections() const
    {
        return m_maxBoostTcpConnections;
    }

    /// How long a datagram transport may stay idle before the user is
    /// considered disconnected. A non-positive value disables idle expiry.
    void setDatagramIdleTimeout(std::chrono::milliseconds timeout)
    {
        m_datagramIdleTimeout = timeout;
    }
    std::chrono::milliseconds getDatagramIdleTimeout() const
    {
        return m_datagramIdleTimeout;
    }

    /// Gameplay limits enforced on player actions.
    GameplayLimits& getGameplayLimits()
    {
        return m_gameplayLimits;
    }
    const GameplayLimits& getGameplayLimits() const
    {
        return m_gameplayLimits;
    }

    /// Limits enforced on client-requested room creation.
    RoomLimits& getRoomLimits()
    {
        return m_roomLimits;
    }
    const RoomLimits& getRoomLimits() const
    {
        return m_roomLimits;
    }

    /// Whether Boost TCP uses certificate-based TLS. On by default.
    bool isTlsEnabled() const
    {
        return m_tls;
    }

    void setTls(bool enabled)
    {
        m_tls = enabled;
    }

    /// Install a PEM certificate and its private key. Keep the key on the
    /// server and publish only the certificate's public-key SHA-256 pin.
    void setTlsIdentity(std::string certificatePem, std::string privateKeyPem)
    {
        m_tlsCertificatePem = std::move(certificatePem);
        m_tlsPrivateKeyPem = std::move(privateKeyPem);
    }
    const std::string& getTlsCertificatePem() const
    {
        return m_tlsCertificatePem;
    }
    const std::string& getTlsPrivateKeyPem() const
    {
        return m_tlsPrivateKeyPem;
    }

private:
    std::string m_rootPassword;
    float m_tickrate = 60.f;
    float m_maxCommandRate = NEXILIS_DEFAULT_MAX_COMMAND_RATE;
    std::size_t m_maxPendingBoostTcpConnections = 64;
    std::size_t m_maxBoostTcpConnections = 512;
    std::chrono::milliseconds m_datagramIdleTimeout = std::chrono::seconds(120);
    bool m_tls = true;
    std::string m_tlsCertificatePem;
    std::string m_tlsPrivateKeyPem;
    GameplayLimits m_gameplayLimits;
    RoomLimits m_roomLimits;
};

} // namespace nexilis::server

#endif
