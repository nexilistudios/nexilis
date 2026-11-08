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

#include <nexilis/server/command/command.hh>
#include <nexilis/server/command/commands.hh>
#include <nexilis/server/room_storage.hh>

namespace nexilis::server
{

CommandResult ServerImpl::room_management_leave(const DefaultArgs& args)
{
    auto& user = args.getUser();
    auto currentRoom = RoomStorage::getRoomById(user.getRoomId());

    if (!currentRoom)
    {
        Log::warning("Client not currently in room so cannot leave current room.");
        return CommandResult::failure;
    }

    currentRoom->leaveRoom(user.getId());

    std::map<std::string, boost::json::value> params;
    auto roomCommand = Command::createRoomCommand(currentRoom->getId(), user, args.getData(), params, args.getMessageId());
    Command::sendRoomCommand(roomCommand, user, args.getProtocol());
    // The leaver has already been removed from the room, so the broadcast
    // above cannot update its local room state. Send it the leave event too.
    Command::sendMessageToClient(roomCommand, user, args.getProtocol());

    user.setRoomId(0);
    return CommandResult::success;
}

} // namespace nexilis::server
