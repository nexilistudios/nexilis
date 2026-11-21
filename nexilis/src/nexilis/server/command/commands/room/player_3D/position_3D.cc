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
    const float rawYaw = payload.size() == 16
                                 ? Util::floatFromFront(Util::removeAmountOfBytesFromVector(payload, 12))
                                 : 0.0f;
    if (!std::isfinite(vector.x) || !std::isfinite(vector.y) ||
        !std::isfinite(vector.z) || !std::isfinite(rawYaw))
        return CommandResult::invalid_input;

    const auto& limits = GameplayLimits::get();
    if (limits.clampHorizontal(vector.x) != vector.x ||
        limits.clampHeight(vector.y) != vector.y ||
        limits.clampHorizontal(vector.z) != vector.z)
        return CommandResult::invalid_input;
    Log::debug("Position x:", vector.x, " y:", vector.y, " z:", vector.z);

    // Normalize yaw into [-pi, pi] so a client cannot store or broadcast
    // unbounded angles (which break client-side orientation math).
    const float yaw = GameplayLimits::normalizeYaw(rawYaw);
    if (yaw != rawYaw)
    {
        Log::warning("Yaw ", rawYaw, " normalized to ", yaw);
    }

    auto currentRoom = RoomStorage::getRoomById(user.getRoomId());

    if (!currentRoom)
    {
        Log::warning("Client not currently in room.");
        return CommandResult::failure;
    }
    if (!currentRoom->isClientPositionAllowed())
        return CommandResult::invalid_input;
    if (!currentRoom->canSetPosition3D(user.getId(), vector, yaw))
        return CommandResult::unauthorized;

    user.getObject3D().setPosition(vector);
    user.getObject3D().setYaw(yaw);

    std::map<std::string, boost::json::value> params{
            {"x", boost::json::value(vector.x)},
            {"y", boost::json::value(vector.y)},
            {"z", boost::json::value(vector.z)},
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
