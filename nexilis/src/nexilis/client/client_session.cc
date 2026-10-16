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

#include <nexilis/client/client_session.hh>
#include <nexilis/logger/file_log.hh>
#include <nexilis/types/vector2.hh>

namespace nexilis::client
{

ClientSession::ClientSession(uint64_t id, ClientAPI* clientAPI)
    : BaseClient(id),
      m_clientAPI(clientAPI)
{
}

ClientSession::ClientSession(ClientSession&& other) noexcept
    : BaseClient(std::move(other)),
      m_clientAPI(std::move(other.m_clientAPI))
{
}

ClientSession& ClientSession::operator=(ClientSession&& other) noexcept
{
    if (this != &other)
    {
        BaseClient::operator=(std::move(other));
    }
    return *this;
}

bool operator==(const ClientSession& lhs, const ClientSession& rhs)
{
    return lhs.getId() == rhs.getId() &&
           lhs.getUsername() == rhs.getUsername();
}

Vector3f ClientSession::getPosition3D()
{
    auto pos = BaseClient::getObject3D().getPosition();
    std::stringstream ss;
    ss << "Position in ClientSessionCPP x: " << pos.x << " y:" << pos.y << " z:" << pos.z;
    FileLog::debug(ss.str());
    return pos;
}

void ClientSession::setPosition3D(float x, float y, float z)
{
    std::stringstream ss;
    ss << "Setting new position in ClientSessionCPP x: " << x << " y:" << y << " z:" << z;
    FileLog::debug(ss.str());
    BaseClient::getObject3D().setPosition(Vector3<float>(x, y, z));
}

void ClientSession::setPosition2D(float x, float y)
{
    std::stringstream ss;
    ss << "Setting new 2D position in ClientSessionCPP x: " << x << " y:" << y;
    FileLog::debug(ss.str());
    BaseClient::getObject2D().setPosition(Vector2<float>(x, y));
}

Vector2<float> ClientSession::getPosition2D()
{
    auto pos = BaseClient::getObject2D().getPosition();
    std::stringstream ss;
    ss << "Position in ClientSessionCPP2D x: " << pos.x << " y:" << pos.y;
    FileLog::debug(ss.str());
    return pos;
}

} // namespace nexilis::client
