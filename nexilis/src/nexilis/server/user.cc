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

namespace nexilis::server
{

User::User(uint64_t id, std::string ip_address) noexcept
    : BaseClient(id),
      m_ip_address(ip_address)
{
}

User::User(User&& other) noexcept
    : BaseClient(std::move(other)),
      m_ip_address(std::move(other.m_ip_address)),
      m_username(std::move(other.m_username)),
      m_roomId(std::move(other.m_roomId)),
      m_lastShotTime(other.m_lastShotTime),
      m_boostTCPSendToClient(std::move(other.m_boostTCPSendToClient)),
      m_boostUDPSendToClient(std::move(other.m_boostUDPSendToClient)),
      m_unixStreamSendToClient(std::move(other.m_unixStreamSendToClient)),
      m_unixDgramSendToClient(std::move(other.m_unixDgramSendToClient)),
      m_inetTCPSendToClient(std::move(other.m_inetTCPSendToClient)),
      m_inetUDPSendToClient(std::move(other.m_inetUDPSendToClient)),
      m_hasRootAccess(other.m_hasRootAccess.load()),
      m_hasCommonAccess(other.m_hasCommonAccess.load())
{
}

/// Move assignment operator.
User& User::operator=(User&& other) noexcept
{
    if (this != &other)
    {
        m_ip_address = std::move(other.m_ip_address);
        m_username = std::move(other.m_username);
        m_roomId = std::move(other.m_roomId);
        m_lastShotTime = other.m_lastShotTime;
        m_boostTCPSendToClient = std::move(other.m_boostTCPSendToClient);
        m_boostUDPSendToClient = std::move(other.m_boostUDPSendToClient);
        m_unixStreamSendToClient = std::move(other.m_unixStreamSendToClient);
        m_unixDgramSendToClient = std::move(other.m_unixDgramSendToClient);
        m_inetTCPSendToClient = std::move(other.m_inetTCPSendToClient);
        m_inetUDPSendToClient = std::move(other.m_inetUDPSendToClient);
        m_hasRootAccess.store(other.m_hasRootAccess.load());
        m_hasCommonAccess.store(other.m_hasCommonAccess.load());

        BaseClient::operator=(std::move(other));
    }
    return *this;
}

bool User::boostTCPSend(const nx_data& data)
{
    if (m_boostTCPSendToClient)
    {
        (m_boostTCPSendToClient)(data);
        return true;
    }
    return false;
}

bool User::boostUDPSend(const nx_data& data)
{
    if (m_boostUDPSendToClient)
    {
        (m_boostUDPSendToClient)(data);
        return true;
    }
    return false;
}

bool User::unixStreamSend(const nx_data& data)
{
    if (m_unixStreamSendToClient)
    {
        (m_unixStreamSendToClient)(data);
        return true;
    }
    return false;
}

bool User::unixDgramSend(const nx_data& data)
{
    if (m_unixDgramSendToClient)
    {
        (m_unixDgramSendToClient)(data);
        return true;
    }
    return false;
}

bool User::inetTCPSend(const nx_data& data)
{
    if (m_inetTCPSendToClient)
    {
        (m_inetTCPSendToClient)(data);
        return true;
    }
    return false;
}

bool User::inetUDPSend(const nx_data& data)
{
    if (m_inetUDPSendToClient)
    {
        (m_inetUDPSendToClient)(data);
        return true;
    }
    return false;
}

} // namespace nexilis::server
