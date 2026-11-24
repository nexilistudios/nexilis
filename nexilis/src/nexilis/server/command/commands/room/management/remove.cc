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

#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/command/commands.hh>
#include <nexilis/server/room_storage.hh>

#include <algorithm>

namespace nexilis::server
{

namespace
{
// Room removal is a server-originated invalidation for an existing member.
// Prefer a reliable connection; a datagram transport is used only when the
// peer has no stream callback. Request replies still use their own transport.
bool sendRemovalToPeer(const nx_data& data, User& peer)
{
    if (peer.isBoostTCPSet())
        return peer.boostTCPSend(data);
    if (peer.isUnixStreamSet())
        return peer.unixStreamSend(data);
    if (peer.isInetTCPSet())
        return peer.inetTCPSend(data);
    if (peer.isBoostUDPSet())
        return peer.boostUDPSend(data);
    if (peer.isUnixDgramSet())
        return peer.unixDgramSend(data);
    if (peer.isInetUDPSet())
        return peer.inetUDPSend(data);
    return false;
}
} // namespace

CommandResult ServerImpl::room_management_remove(const DefaultArgs& args)
{
    auto payload = Util::removeAmountOfBytesFromVector(args.getData(), 3);
    if (payload.size() < sizeof(uint64_t))
    {
        Log::error("Remove room: Insufficient payload");
        return CommandResult::invalid_input;
    }
    uint64_t roomId = Util::convertToType<uint64_t>(payload);

    auto room = RoomStorage::getRoomById(roomId);

    if (!room)
    {
        Log::error("Cannot find room with specified id!");
        return CommandResult::invalid_input;
    }

    // Only the creator can remove the room, and client-requested removal is
    // disabled unless the room explicitly opts in.
    if (room->getCreatorId() != args.getUser().getId())
    {
        Log::error("User does not have permission to remove room!");
        return CommandResult::unauthorized;
    }
    if (!room->isClientRoomRemovalAllowed())
    {
        Log::warning("Client room removal is disabled for room ", roomId);
        return CommandResult::unauthorized;
    }

    // Keep the member list while the room is still published. Unpublish it
    // before notification so no new client can join a room being removed.
    auto roomClients = room->getClients();
    const bool actorIsMember = std::find(roomClients.begin(), roomClients.end(),
                                         args.getUser().getId()) != roomClients.end();
    if (!RoomStorage::remove(roomId))
        return CommandResult::invalid_input;

    auto& user = args.getUser();
    auto roomCommand = Command::createRoomCommand(roomId, user, args.getData(), {},
                                                  args.getMessageId());
    // A peer must not receive the creator's request callback ID.
    auto peerCommand = Command::createRoomCommand(roomId, user, args.getData(), {}, 0);
    for (auto clientId : roomClients)
    {
        auto storedClient = ClientStorage::getClientById(clientId);
        User* client = clientId == user.getId() ? &user : storedClient.get();
        if (client && client->getRoomId() == roomId)
        {
            try
            {
                if (clientId == user.getId())
                    Command::sendMessageToClient(roomCommand, *client, args.getProtocol());
                else
                    sendRemovalToPeer(peerCommand, *client);
            }
            catch (...)
            {
                Log::warning("Room removal notification failed for user ", clientId);
            }
        }
    }
    if (!actorIsMember)
    {
        try
        {
            Command::sendMessageToClient(roomCommand, user, args.getProtocol());
        }
        catch (...)
        {
            Log::warning("Room removal response failed for user ", user.getId());
        }
    }

    // Clear membership after notification, without overwriting a concurrent
    // transition to another room.
    for (auto clientId : roomClients)
    {
        auto storedClient = ClientStorage::getClientById(clientId);
        User* client = clientId == user.getId() ? &user : storedClient.get();
        if (!client)
            continue;
        std::lock_guard lock(client->roomTransitionMutex());
        if (client->getRoomId() == roomId)
            client->setRoomId(0);
    }
    if (!actorIsMember)
    {
        std::lock_guard lock(user.roomTransitionMutex());
        if (user.getRoomId() == roomId)
            user.setRoomId(0);
    }

    return CommandResult::success;
}

} // namespace nexilis::server
