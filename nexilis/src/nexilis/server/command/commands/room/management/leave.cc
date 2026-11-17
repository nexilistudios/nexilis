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

namespace nexilis::server
{

CommandResult ServerImpl::room_management_leave(const DefaultArgs& args)
{
    auto& user = args.getUser();
    std::shared_ptr<Room> currentRoom;
    {
        std::lock_guard lock(user.roomTransitionMutex());
        currentRoom = RoomStorage::getRoomById(user.getRoomId());
        if (!currentRoom || !currentRoom->contains(user.getId()))
        {
            Log::warning("Client not currently in room so cannot leave current room.");
            return CommandResult::failure;
        }
        currentRoom->leaveRoom(user.getId());
        user.setRoomId(0);
    }

    std::map<std::string, boost::json::value> params;
    auto roomCommand = Command::createRoomCommand(currentRoom->getId(), user, args.getData(), params, args.getMessageId());
    for (auto clientId : currentRoom->getClients())
    {
        auto client = ClientStorage::getClientById(clientId);
        if (client && client->getRoomId() == currentRoom->getId())
            Command::sendMessageToClient(roomCommand, *client, args.getProtocol());
    }
    Command::sendMessageToClient(roomCommand, user, args.getProtocol());

    return CommandResult::success;
}

} // namespace nexilis::server
