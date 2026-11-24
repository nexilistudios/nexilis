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
#include <functional>
#include <string>
#include <string_view>

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

    /// Validate a stable account ID and private credential after the shared
    /// passphrase. Required for password-protected Boost TCP and configured
    /// before starting the server. Password-only handshakes are refused.
    using AccountAuthenticator = std::function<bool(std::string_view, std::string_view)>;
    void setAccountAuthenticator(AccountAuthenticator authenticator)
    {
        m_accountAuthenticator = std::move(authenticator);
    }
    const AccountAuthenticator& getAccountAuthenticator() const
    {
        return m_accountAuthenticator;
    }
    /// Maximum concurrent sessions for one authenticated account.
    void setMaxSessionsPerAccount(std::size_t maximum)
    {
        m_maxSessionsPerAccount = maximum;
    }
    std::size_t getMaxSessionsPerAccount() const
    {
        return m_maxSessionsPerAccount;
    }

    /// Maximum live clients admitted by this server. The registry's compile-time
    /// maximum remains an upper bound. Zero refuses new clients.
    void setMaxClients(std::size_t maximum)
    {
        m_maxClients = maximum;
    }
    std::size_t getMaxClients() const
    {
        return m_maxClients;
    }

    /// Keep this many client slots for priority accounts. Configure the
    /// priority predicate before admitting clients; without it, no account
    /// qualifies for the reserved capacity.
    void setReservedClientSlots(std::size_t count)
    {
        m_reservedClientSlots = count;
    }
    std::size_t getReservedClientSlots() const
    {
        return m_reservedClientSlots;
    }

    /// Classify an already authenticated account for reserved-slot admission.
    using PriorityAccountPredicate = std::function<bool(std::string_view)>;
    void setPriorityAccountPredicate(PriorityAccountPredicate predicate)
    {
        m_priorityAccountPredicate = std::move(predicate);
    }
    const PriorityAccountPredicate& getPriorityAccountPredicate() const
    {
        return m_priorityAccountPredicate;
    }

    /// Maximum live sessions from one source address. Zero refuses new clients.
    /// Address limits also apply when account authentication is configured.
    void setMaxSessionsPerAddress(std::size_t maximum)
    {
        m_maxSessionsPerAddress = maximum;
    }
    std::size_t getMaxSessionsPerAddress() const
    {
        return m_maxSessionsPerAddress;
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
    AccountAuthenticator m_accountAuthenticator;
    std::size_t m_maxSessionsPerAccount = 1;
    std::size_t m_maxClients = NEXILIS_DEFAULT_MAX_CLIENTS;
    std::size_t m_reservedClientSlots = 0;
    PriorityAccountPredicate m_priorityAccountPredicate;
    std::size_t m_maxSessionsPerAddress = 8;
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
