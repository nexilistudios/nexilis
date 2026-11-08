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
#include <nexilis/server/gameplay_limits.hh>
#include <nexilis/server/room_storage.hh>

#include <chrono>

namespace nexilis::server
{

CommandResult ServerImpl::room_communicate_broadcast(const DefaultArgs& args)
{
    auto& user = args.getUser();
    const auto& limits = GameplayLimits::get();
    if (args.getData().size() < 3 ||
        args.getData().size() - 3 > limits.getMaxBroadcastLength())
    {
        Log::warning("Broadcast: player ", user.getId(),
                     " sent a message longer than the ",
                     limits.getMaxBroadcastLength(), " byte limit");
        return CommandResult::invalid_input;
    }

    auto messageData = Util::convertToString(
            Util::removeAmountOfBytesFromVector(args.getData(), 3));

    if (user.getRoomId() == 0)
    {
        Log::error("User not currently in room!");
        return CommandResult::error;
    }

    auto room = RoomStorage::getRoomById(user.getRoomId());
    if (!room)
    {
        Log::error("Cannot find room for broadcast!");
        return CommandResult::error;
    }

    const auto interval = limits.broadcastInterval();
    if (interval.count() > 0.0f &&
        !user.acceptBroadcast(std::chrono::steady_clock::now(), interval))
    {
        Log::warning("Broadcast: player ", user.getId(),
                     " exceeded the broadcast rate limit of ",
                     limits.getMaxBroadcastRate(), " messages per second");
        return CommandResult::invalid_input;
    }

    room->addBroadcast(user.getId(), messageData);

    std::map<std::string, boost::json::value> params{
            {"id", boost::json::value(user.getId())},
            {"room_id", boost::json::value(user.getRoomId())},
            {"message", boost::json::value(messageData)}};

    auto roomCommand = Command::createRoomCommand(user.getRoomId(), user, args.getData(), params, args.getMessageId());
    Command::sendRoomCommand(roomCommand, user, args.getProtocol());
    return CommandResult::success;
}
} // namespace nexilis::server
