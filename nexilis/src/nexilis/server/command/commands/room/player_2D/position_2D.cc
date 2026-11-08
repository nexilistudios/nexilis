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

#include <nexilis/server/command/commands.hh>

#include <nexilis/server/command/command.hh>
#include <nexilis/server/gameplay_limits.hh>
#include <nexilis/server/room_storage.hh>

#include <cmath>

namespace nexilis::server
{

CommandResult ServerImpl::room_player2d_position(const DefaultArgs& args)
{
    auto& user = args.getUser();

    if (user.getRoomId() == 0)
    {
        Log::error("User not in room!");
        return CommandResult::error;
    }

    auto payload = Util::removeAmountOfBytesFromVector(args.getData(), 3);
    if (payload.size() != sizeof(float) * 2)
        return CommandResult::invalid_input;
    auto vector = Util::convertToVector2(payload);

    if (!std::isfinite(vector.x) || !std::isfinite(vector.y))
        return CommandResult::invalid_input;

    const auto& limits = GameplayLimits::get();
    const Vector2f position(limits.clampHorizontal(vector.x),
                            limits.clampHorizontal(vector.y));
    Log::debug("Position x:", position.x, " y:", position.y);

    auto currentRoom = RoomStorage::getRoomById(user.getRoomId());

    if (!currentRoom)
    {
        Log::warning("Client not currently in room.");
        return CommandResult::failure;
    }

    if (!currentRoom->isClientPositionAllowed())
        return CommandResult::invalid_input;

    user.getObject2D().setPosition(position);

    std::map<std::string, boost::json::value> params{
            {"x", boost::json::value(position.x)},
            {"y", boost::json::value(position.y)}};

    auto roomCommand = Command::createRoomCommand(user.getRoomId(), user, args.getData(), params, args.getMessageId());
    if (Command::sendRoomCommand(roomCommand, user, args.getProtocol()))
    {
        return CommandResult::success;
    }
    else
    {
        return CommandResult::failure;
    }
}

} // namespace nexilis::server
