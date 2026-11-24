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
#include <nexilis/util.hh>

namespace nexilis::server
{

CommandResult ServerImpl::room_player3d_setTeam(const DefaultArgs& args)
{
    auto& user = args.getUser();
    if (user.getRoomId() == 0)
    {
        Log::error("Set team: User not in room!");
        return CommandResult::error;
    }

    auto payload = Util::removeAmountOfBytesFromVector(args.getData(), 3);
    std::string team = Util::convertToString(payload);

    if (team.empty() || team.size() > 64 || team.find('\0') != std::string::npos)
    {
        Log::warning("Set team: Unknown team '", team, "' from user ", user.getId());
        return CommandResult::invalid_input;
    }

    auto room = RoomStorage::getRoomById(user.getRoomId());
    if (!room)
    {
        Log::error("Set team: Room not found");
        return CommandResult::failure;
    }

    if (!room->canSelectTeam(user.getId(), team))
        return CommandResult::invalid_input;

    room->setPlayerTeam(user.getId(), team);
    Log::info("Player ", user.getId(), " [", user.getUsername(), "] joined team ", team);

    // Announce the joining player's stats to the whole room so every client
    // can keep its local leaderboard table up to date...
    auto statsPayload = Command::createRoomLeaderboardCommand(
            room->getId(), user,
            Command::playerStatsEntries(*room, {user.getId()}), 0);
    room->broadcastToAll(statsPayload);

    // ...and seed the joining player with everyone currently in the room.
    auto fullPayload = Command::createRoomLeaderboardCommand(
            room->getId(), user,
            Command::playerStatsEntries(*room, room->getClients()),
            args.getMessageId());
    Command::sendMessageToClient(fullPayload, user, args.getProtocol());

    return CommandResult::success;
}

} // namespace nexilis::server
