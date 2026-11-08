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

#include <nexilis/command_type.hh>
#include <nexilis/logger/log.hh>
#include <nexilis/room_command_type.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/command/commands.hh>
#include <nexilis/server/room_storage.hh>
#include <nexilis/server/validation.hh>
#include <nexilis/util.hh>

namespace nexilis::server
{

CommandResult ServerImpl::room_player3d_audioEvent(const DefaultArgs& args)
{
    auto& user = args.getUser();
    if (user.getRoomId() == 0)
    {
        Log::error("AudioEvent: User not in room!");
        return CommandResult::error;
    }

    // Payload: [sound (1 byte)][x (4)][y (4)][z (4)]
    auto payload = Util::removeAmountOfBytesFromVector(args.getData(), 3);
    if (payload.size() != 13)
    {
        Log::error("AudioEvent: Insufficient payload");
        return CommandResult::invalid_input;
    }

    const uint8_t sound = payload[0];
    payload = Util::removeAmountOfBytesFromVector(payload, 1);
    auto position = Util::vector3fFromFront(payload);
    if (!validation::isValidPosition(position))
        return CommandResult::invalid_input;

    auto room = RoomStorage::getRoomById(user.getRoomId());
    if (!room)
    {
        Log::error("AudioEvent: Room not found");
        return CommandResult::failure;
    }

    // Relay the event to the whole room; the emitting client ignores its own
    // event (it already plays the sound itself, spatially centered).
    std::map<std::string, boost::json::value> params{
            {"sound", boost::json::value(static_cast<uint64_t>(sound))},
            {"x", boost::json::value(position.x)},
            {"y", boost::json::value(position.y)},
            {"z", boost::json::value(position.z)}};

    if (Command::sendRoomCommand(
                Command::createRoomCommand(
                        user.getRoomId(), user, args.getData(), params, args.getMessageId()),
                user, args.getProtocol()) == false)
        return CommandResult::failed_room_send;

    return CommandResult::success;
}

} // namespace nexilis::server
