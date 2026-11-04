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

CommandResult ServerImpl::room_management_remove(const DefaultArgs& args)
{
    auto payload = Util::removeAmountOfBytesFromVector(args.getData(), 3);
    if (payload.size() < sizeof(uint64_t))
    {
        Log::error("Remove room: Insufficient payload");
        return CommandResult::invalid_input;
    }
    uint64_t roomId = Util::convertToType<uint64_t>(payload);

    auto* room = RoomStorage::getRoomById(roomId);

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

    // Remove all clients from the room first.
    auto roomClients = room->getClients();
    for (auto clientId : roomClients)
    {
        auto* client = ClientStorage::getClientById(clientId);
        if (client)
        {
            client->setRoomId(0);
        }
    }

    // Remove the room from storage.
    auto& rooms = RoomStorage::getAllRooms();
    rooms.erase(std::remove_if(rooms.begin(), rooms.end(),
                               [roomId](const Room& r)
                               {
                                   return r.getId() == roomId;
                               }),
                rooms.end());

    std::map<std::string, boost::json::value> params{
            {"action", boost::json::value("remove")},
            {"room_id", boost::json::value(roomId)}};

    auto roomCommand = Command::createRoomCommand(roomId, args.getUser(), args.getData(), params, args.getMessageId());
    Command::sendRoomCommand(roomCommand, args.getUser(), args.getProtocol());

    return CommandResult::success;
}

} // namespace nexilis::server
