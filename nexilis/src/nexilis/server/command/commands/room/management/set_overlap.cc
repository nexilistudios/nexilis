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

#include <nexilis/logger/log.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/command/commands.hh>
#include <nexilis/server/room_storage.hh>

namespace nexilis::server
{

CommandResult ServerImpl::room_management_setOverlap(const DefaultArgs& args)
{
    Log::debug("Commands::Room::Management::setOverlap", args);
    auto& user = args.getUser();

    auto payload = Util::removeAmountOfBytesFromVector(args.getData(), 3);
    if (payload.empty())
    {
        Log::error("Set overlap: Insufficient payload");
        return CommandResult::invalid_input;
    }
    bool allowed = payload.front() != 0;

    auto room = RoomStorage::getRoomById(user.getRoomId());
    if (!room)
    {
        Log::error("Set overlap: User not in room!");
        return CommandResult::error;
    }

    // Overlap is a room-wide setting, so only the creator may change it.
    if (room->getCreatorId() != user.getId())
    {
        Log::warning("Set overlap: User ", user.getId(),
                     " is not the creator of room ", room->getId());
        return CommandResult::unauthorized;
    }

    room->setOverlappingAllowed(allowed);
    Log::info("Room ", room->getId(), " overlapping set to ", allowed);

    std::map<std::string, boost::json::value> params{
            {"overlap_allowed", boost::json::value(allowed)}};

    // Notify the room so peers can mirror the new overlap status.
    auto roomCommand = Command::createRoomCommand(room->getId(), user, args.getData(),
                                                  params, args.getMessageId());
    Command::sendRoomCommand(roomCommand, user, args.getProtocol());

    return CommandResult::success;
}

} // namespace nexilis::server
