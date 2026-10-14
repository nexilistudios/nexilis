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

#include <cmath>

namespace nexilis::server
{

CommandResult ServerImpl::room_player3d_position(const DefaultArgs& args)
{
    auto& user = args.getUser();

    if (user.getRoomId() == 0)
    {
        Log::error("User not in room!");
        return CommandResult::error;
    }

    auto payload = Util::removeAmountOfBytesFromVector(args.getData(), 3);
    Log::debug("Payload size: ", payload.size());
    if (payload.size() != 12 && payload.size() != 16)
        return CommandResult::invalid_input;
    auto vector = Util::convertToVector3(payload);
    const float yaw = payload.size() == 16
                              ? Util::floatFromFront(Util::removeAmountOfBytesFromVector(payload, 12))
                              : 0.0f;
    if (!std::isfinite(vector.x) || !std::isfinite(vector.y) ||
        !std::isfinite(vector.z) || !std::isfinite(yaw))
        return CommandResult::invalid_input;

    // Keep the position inside the configured world limits: the
    // horizontal coordinates and the height are clamped to the
    // nearest allowed value.
    const auto& limits = GameplayLimits::get();
    Vector3f position(limits.clampHorizontal(vector.x),
                      limits.clampHeight(vector.y),
                      limits.clampHorizontal(vector.z));
    if (position.x != vector.x || position.y != vector.y || position.z != vector.z)
    {
        Log::warning("Position (", vector.x, ", ", vector.y, ", ", vector.z,
                     ") clamped to (", position.x, ", ", position.y, ", ", position.z, ")");
    }
    Log::debug("Position x:", position.x, " y:", position.y, " z:", position.z);

    auto currentRoom = RoomStorage::getRoomById(user.getRoomId());

    if (!currentRoom)
    {
        Log::warning("Client not currently in room.");
        return CommandResult::failure;
    }
    if (!currentRoom->isClientPositionAllowed())
        return CommandResult::invalid_input;

    user.getObject3D().setPosition(position);
    user.getObject3D().setYaw(yaw);

    std::map<std::string, boost::json::value> params{
            {"x", boost::json::value(position.x)},
            {"y", boost::json::value(position.y)},
            {"z", boost::json::value(position.z)},
            {"yaw", boost::json::value(yaw)}};

    auto roomCommand = Command::createRoomCommand(user.getRoomId(), user, args.getData(), params, args.getMessageId());
    if (Command::sendRoomCommand(roomCommand, user, args.getProtocol()))
    {
        return CommandResult::success;
    }
    else
    {
        return CommandResult::failed_room_send;
    }
}

} // namespace nexilis::server
