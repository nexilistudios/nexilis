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
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/command/commands.hh>
#include <nexilis/server/room_storage.hh>

#include <cmath>

namespace nexilis::server
{

CommandResult ServerImpl::room_player2d_shoot(const DefaultArgs& args)
{
    auto& user = args.getUser();
    auto data = args.getData();
    auto& protocol = args.getProtocol();
    auto messageId = args.getMessageId();

    // Get messagedata: direction vector (x, y)
    auto payload = Util::removeAmountOfBytesFromVector(data, 3);
    auto dirX = Util::floatFromFront(payload);
    auto dirY = Util::floatFromFront(Util::removeAmountOfBytesFromVector(payload, 4));
    Vector2f direction(dirX, dirY);

    // Normalize direction
    float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length > 0.0f)
    {
        direction.x /= length;
        direction.y /= length;
    }

    if (length == 0.0f)
        return CommandResult::failure;

    // Clients simulate one visual projectile per shoot event. Broadcast its direction.
    std::map<std::string, boost::json::value> params{
            {"x", boost::json::value(direction.x)},
            {"y", boost::json::value(direction.y)}};
    auto roomCommand = Command::createRoomCommand(user.getRoomId(), user, data, params, messageId);
    Command::sendRoomCommand(roomCommand, user, protocol);

    return CommandResult::success;
}

} // namespace nexilis::server
