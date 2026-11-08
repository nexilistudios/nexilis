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

    /// Whether TCP connections are encrypted with TLS-PSK.
    /// \note The pre-shared key is derived from the passphrase, so a client
    ///       only connects when it knows the same passphrase. On by default
    ///       for Boost TCP; a passphrase is required unless disabled.
    bool isTlsEnabled() const
    {
        return m_tls;
    }

    void setTls(bool enabled)
    {
        m_tls = enabled;
    }

private:
    std::string m_rootPassword;
    float m_tickrate = 60.f;
    float m_maxCommandRate = NEXILIS_DEFAULT_MAX_COMMAND_RATE;
    bool m_tls = true;
    GameplayLimits m_gameplayLimits;
    RoomLimits m_roomLimits;
};

} // namespace nexilis::server

#endif
