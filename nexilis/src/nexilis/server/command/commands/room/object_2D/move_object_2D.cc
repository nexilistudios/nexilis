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
#include <nexilis/server/validation.hh>

namespace nexilis::server
{

CommandResult ServerImpl::room_object2d_move(const DefaultArgs& args)
{
    auto& user = args.getUser();
    auto room = RoomStorage::getRoomById(user.getRoomId());
    if (!room || !room->isClientObjectCreationAllowed())
        return CommandResult::failure;

    // Get messagedata.
    auto payload = Util::removeAmountOfBytesFromVector(args.getData(), 3);
    if (payload.size() != 16)
        return CommandResult::invalid_input;
    auto objectId = Util::uint64FromFront(payload);
    auto position = Util::vector2fFromFront(Util::removeAmountOfBytesFromVector(payload, 8));
    if (!validation::isFinite(position))
        return CommandResult::invalid_input;

    const auto& limits = GameplayLimits::get();
    Vector2f clampedPosition;
    bool validPosition = false;
    const bool found = room->withObject2D(objectId, [&](Object2D& object)
                                          {
        const auto newPosition = object.getPosition() + position;
        if (!validation::isFinite(newPosition))
            return;
        clampedPosition = Vector2f(limits.clampHorizontal(newPosition.x),
                                   limits.clampHorizontal(newPosition.y));
        object.setPosition(clampedPosition);
        validPosition = true; });
    if (!found)
        return CommandResult::failure;
    if (!validPosition)
        return CommandResult::invalid_input;

    std::map<std::string, boost::json::value> params{
            {"id", boost::json::value(objectId)},
            {"x", boost::json::value(clampedPosition.x)},
            {"y", boost::json::value(clampedPosition.y)}};

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
