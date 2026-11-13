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

#include <nexilis/server/server_protocol.hh>

#include <nexilis/room_command_type.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/commands.hh>
#include <nexilis/server/room_storage.hh>

#include <algorithm>

namespace nexilis::server
{

namespace
{
nx_data leaveRoomData()
{
    return {
            static_cast<uint8_t>(CommandType::room),
            static_cast<uint8_t>(RoomCommandType::Root::management),
            static_cast<uint8_t>(RoomCommandType::Management::leave)};
}
} // namespace

ServerProtocol::ServerProtocol(const ServerConfig& settings)
    : m_command(settings)
{
}

ServerProtocol::ServerProtocol(ServerProtocol&& other)
    : m_messageHandler(std::move(other.m_messageHandler)),
      m_command(std::move(other.m_command)),
      m_activeConnections(other.m_activeConnections.load()),
      m_lastDatagramSweep(other.m_lastDatagramSweep)
{
}

ServerProtocol& ServerProtocol::operator=(ServerProtocol&& other)
{
    if (this != &other)
    {
        m_messageHandler = std::move(other.m_messageHandler);
        m_command = std::move(other.m_command);
        m_activeConnections = std::move(other.m_activeConnections.load());
        m_lastDatagramSweep = other.m_lastDatagramSweep;
    }
    return *this;
}

void ServerProtocol::registerStreamTransport(User& user, User::Transport transport)
{
    user.registerStreamTransport(transport);
}

void ServerProtocol::touchDatagramTransport(User& user, User::Transport transport)
{
    user.touchDatagramTransport(transport, std::chrono::steady_clock::now());
}

void ServerProtocol::streamTransportEnded(User& user, User::Transport transport,
                                          NXProtocol& protocol)
{
    const auto status = user.endStreamTransport(transport);
    if (!status.anyActive)
    {
        leaveAndRemoveUser(user, protocol);
        return;
    }
    if (!status.transportActive)
        user.clearSendCallback(transport);
}

void ServerProtocol::datagramTransportEnded(User& user, User::Transport transport,
                                            NXProtocol& protocol)
{
    const auto status = user.endDatagramTransport(transport);
    if (!status.anyActive)
    {
        leaveAndRemoveUser(user, protocol);
        return;
    }
    if (!status.transportActive)
        user.clearSendCallback(transport);
}

void ServerProtocol::beginPendingHandshake(User& user)
{
    user.beginPendingHandshake(std::chrono::steady_clock::now());
}

void ServerProtocol::completePendingHandshake(User& user)
{
    user.endPendingHandshake();
}

void ServerProtocol::abandonPendingHandshake(User& user, NXProtocol& protocol)
{
    const auto status = user.endPendingHandshake();
    if (!status.anyActive)
    {
        leaveAndRemoveUser(user, protocol);
        return;
    }
    if (!status.transportActive)
        user.clearSendCallback(User::Transport::boostTcp);
}

void ServerProtocol::expirePendingHandshakes(NXProtocol& protocol)
{
    const auto now = std::chrono::steady_clock::now();
    const auto users = ClientStorage::getAllClients();
    for (const auto& user : users)
    {
        if (!user)
            continue;

        const auto status = user->expirePendingHandshakes(now, pendingHandshakeTimeout);
        if (!status)
            continue;

        if (!status->anyActive)
            leaveAndRemoveUser(*user, protocol);
        else if (!status->transportActive)
            user->clearSendCallback(User::Transport::boostTcp);
    }
}

void ServerProtocol::cleanupPendingHandshakes(NXProtocol& protocol)
{
    const auto users = ClientStorage::getAllClients();
    for (const auto& user : users)
    {
        if (!user)
            continue;

        const auto status = user->cancelPendingHandshakes();
        if (!status)
            continue;

        if (!status->anyActive)
            leaveAndRemoveUser(*user, protocol);
        else if (!status->transportActive)
            user->clearSendCallback(User::Transport::boostTcp);
    }
}

void ServerProtocol::expireIdleDatagramTransports(User::Transport transport,
                                                  NXProtocol& protocol)
{
    const auto timeout = getSettings().getDatagramIdleTimeout();
    if (timeout <= std::chrono::milliseconds::zero())
        return;

    const auto now = std::chrono::steady_clock::now();
    const auto interval = std::min(std::chrono::milliseconds(1000),
                                   std::max(std::chrono::milliseconds(10), timeout / 2));
    if (m_lastDatagramSweep != std::chrono::steady_clock::time_point{} &&
        now - m_lastDatagramSweep < interval)
        return;
    m_lastDatagramSweep = now;

    const auto users = ClientStorage::getAllClients();
    for (const auto& user : users)
    {
        if (!user)
            continue;

        const auto status = user->expireDatagramTransport(transport, now, timeout);
        if (!status)
            continue;

        if (!status->anyActive)
            leaveAndRemoveUser(*user, protocol);
        else
            user->clearSendCallback(transport);
    }
}

void ServerProtocol::cleanupDatagramTransports(User::Transport transport,
                                               NXProtocol& protocol)
{
    const auto users = ClientStorage::getAllClients();
    for (const auto& user : users)
    {
        if (!user || !user->hasTransport(transport))
            continue;
        datagramTransportEnded(*user, transport, protocol);
    }
}

void ServerProtocol::leaveAndRemoveUser(User& user, NXProtocol& protocol)
{
    try
    {
        if (user.getRoomId() != 0 && RoomStorage::getRoomById(user.getRoomId()))
        {
            const auto leaveData = leaveRoomData();
            DefaultArgs args(user, protocol, leaveData, 0);
            ServerImpl::room_management_leave(args);
        }
    }
    catch (...)
    {
        // A closed transport must not prevent the user from being removed.
    }

    try
    {
        ClientStorage::remove(user.getId());
    }
    catch (...)
    {
    }
}

} // namespace nexilis::server
