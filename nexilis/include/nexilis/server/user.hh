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

#ifndef NEXILIS_SERVER_USER_HH
#define NEXILIS_SERVER_USER_HH

#include <atomic>
#include <chrono>
#include <cstdint>
#include <nexilis/base_client.hh>
#include <nexilis/nexilis_constants.hh>
#include <nexilis/object/object_2d.hh>
#include <nexilis/util.hh>

namespace nexilis::server
{

/// Internal client abstraction for server.
class User : public BaseClient
{
public:
    /// Constructor.
    User(uint64_t id, std::string ip_address) noexcept;

    /// Move constructor.
    User(User&& other) noexcept;

    /// Move assignment operator.
    User& operator=(User&& other) noexcept;

    /// Deleted copy constructor.
    User(const User& other) = delete;

    /// Deleted copy assignment operator.
    User& operator=(const User& other) = delete;

    /// Operator overload for comparison operator.
    bool operator==(const User& other) const
    {
        return m_ip_address == other.getIPAddress();
    }

    /// Getter for the ip address.
    /// \return The ip address of the connection.
    const std::string& getIPAddress() const
    {
        return m_ip_address;
    }

    void setRootAccess(bool hasAccess)
    {
        m_hasRootAccess.store(hasAccess);
    }

    bool hasRootAccess() const
    {
        return m_hasRootAccess.load();
    }

    /// Access redeemed by the passphrase.
    void setCommonAccess(bool hasAccess)
    {
        m_hasCommonAccess.store(hasAccess);
    }

    /// Passphrase has been initialized correctly for server.
    bool hasCommonAccess() const
    {
        return m_hasCommonAccess.load();
    }

    void setId(uint64_t id)
    {
        assert(hasRootAccess());
        BaseClient::setId(id);
    }

    void setRoomId(uint64_t roomId)
    {
        m_roomId = roomId;
    }

    uint64_t getRoomId() const
    {
        return m_roomId;
    }

    /// Time of the last accepted shot. Used to enforce the
    /// shooting rate limit.
    std::chrono::steady_clock::time_point getLastShotTime() const
    {
        return m_lastShotTime;
    }

    void setLastShotTime(std::chrono::steady_clock::time_point time)
    {
        m_lastShotTime = time;
    }

    void setUsername(const std::string& username)
    {
        BaseClient::setBaseUsername(username);
    }

    /// Protocol specific stuff.

    /// \defgroup UserBoostTCP Sending messages via Boost TCP.

    /// Is boost TCP send function set?
    /// \ingroup UserBoostTCP
    bool isBoostTCPSet() const
    {
        return m_boostTCPSendToClient != nullptr;
    }

    /// Set the boost TCP send function.
    /// \ingroup UserBoostTCP
    void setBoostTCPSend(const std::function<void(const nx_data&)>& sendFunction)
    {
        m_boostTCPSendToClient = sendFunction;
    }

    /// Send data using boost TCP.
    /// \ingroup UserBoostTCP
    bool boostTCPSend(const nx_data& data);

    ///\defgroup UserBoostUDP Sending messages via Boost UDP

    /// Is boost UDP send function set?
    /// \ingroup UserBoostUDP
    bool isBoostUDPSet() const
    {
        return m_boostUDPSendToClient != nullptr;
    }

    /// Set the boost UDP send function.
    /// \ingroup UserBoostUDP
    void setBoostUDPSend(const std::function<void(const nx_data&)>& sendFunction)
    {
        m_boostUDPSendToClient = sendFunction;
    }

    /// Send data using boost UDP.
    /// \ingroup UserBoostUDP
    bool boostUDPSend(const nx_data& data);

    /// \defgroup UserUnixStream Send data using unix stream soccets.

    /// Is User unixstream send function set?
    /// \ingroup UserUnixStream
    bool isUnixStreamSet() const
    {
        return m_unixStreamSendToClient != nullptr;
    }

    /// \ingroup UserUnixStream
    void setUnixStreamSend(const std::function<void(const nx_data&)>& sendFunction)
    {
        m_unixStreamSendToClient = sendFunction;
    }

    /// \ingroup UserUnixStream
    bool unixStreamSend(const nx_data& data);

    /// \defgroup UserUnixDgram Send data using unix dgram sockets.

    /// Is the user unix dgram send function set?
    /// \ingroup UserUnixDgram
    bool isUnixDgramSet() const
    {
        return m_unixDgramSendToClient != nullptr;
    }

    /// Set the unix dgram send function.
    /// \ingroup UserUnixDgram
    void setUnixDgramSend(const std::function<void(const nx_data&)>& sendFunction)
    {
        m_unixDgramSendToClient = sendFunction;
    }

    /// Send data using unix dgram.
    /// \ingroup UserUnixDgram
    bool unixDgramSend(const nx_data& data);

    /// \defgroup UserInetTCP Send data using af_inet TCP sockets.

    /// Is the user af_inet TCP send function set?
    /// \ingroup UserInetTCP
    bool isInetTCPSet() const
    {
        return m_inetTCPSendToClient != nullptr;
    }

    /// Set the af_inet TCP send function.
    /// \ingroup UserInetTCP
    void setInetTCPSend(const std::function<void(const nx_data&)>& sendFunction)
    {
        m_inetTCPSendToClient = sendFunction;
    }

    /// Send data using af_inet TCP.
    /// \ingroup UserInetTCP
    bool inetTCPSend(const nx_data& data);

    /// \defgroup UserInetUDP Send data using af_inet UDP sockets.

    /// Is the user af_inet UDP send function set?
    /// \ingroup UserInetUDP
    bool isInetUDPSet() const
    {
        return m_inetUDPSendToClient != nullptr;
    }

    /// Set the af_inet UDP send function.
    /// \ingroup UserInetUDP
    void setInetUDPSend(const std::function<void(const nx_data&)>& sendFunction)
    {
        m_inetUDPSendToClient = sendFunction;
    }

    /// Send data using af_inet UDP.
    /// \ingroup UserInetUDP
    bool inetUDPSend(const nx_data& data);

private:
    // General
    std::string m_ip_address;
    std::string m_username;
    uint64_t m_roomId = 0;
    std::chrono::steady_clock::time_point m_lastShotTime{};

private:
    std::function<void(nx_data)> m_boostTCPSendToClient = nullptr;
    std::function<void(nx_data)> m_boostUDPSendToClient = nullptr;
    std::function<void(nx_data)> m_unixStreamSendToClient = nullptr;
    std::function<void(nx_data)> m_unixDgramSendToClient = nullptr;
    std::function<void(nx_data)> m_inetTCPSendToClient = nullptr;
    std::function<void(nx_data)> m_inetUDPSendToClient = nullptr;

private:
    /// Access area.
    std::atomic<bool> m_hasRootAccess{false};
    std::atomic<bool> m_hasCommonAccess{false};
};

} // namespace nexilis::server

#endif
