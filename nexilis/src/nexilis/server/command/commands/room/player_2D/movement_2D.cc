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

#include <cmath>

namespace nexilis::server
{

namespace
{
constexpr float kMaxClientMovementDelta = 0.25f;
}

CommandResult ServerImpl::room_player2d_movement(const DefaultArgs& args)
{
    const auto& data = args.getData();
    auto payload = Util::removeAmountOfBytesFromVector(data, 3);
    if (payload.size() != 12)
        return CommandResult::invalid_input;

    const float vecX = Util::floatFromFront(payload);
    const float vecY = Util::floatFromFront(Util::removeAmountOfBytesFromVector(payload, 4));
    const float delta = Util::floatFromFront(Util::removeAmountOfBytesFromVector(payload, 8));
    if (!std::isfinite(vecX) || !std::isfinite(vecY) || !std::isfinite(delta) ||
        delta <= 0.0f || delta > kMaxClientMovementDelta)
        return CommandResult::invalid_input;

    auto& user = args.getUser();
    auto* room = RoomStorage::getRoomById(user.getRoomId());
    if (!room)
        return CommandResult::failure;

    // The 2D vector is the total displacement for the requested duration.
    // Apply it during dispatch so no worker outlives the user or room.
    const auto startPosition = user.getObject2D().getPosition();
    const Vector2f newPosition(startPosition.x + vecX, startPosition.y + vecY);
    if (!std::isfinite(newPosition.x) || !std::isfinite(newPosition.y))
        return CommandResult::invalid_input;

    const auto dimensions = user.getObject2D().getDimensions();
    for (auto clientId : room->getClients())
    {
        auto* other = ClientStorage::getClientById(clientId);
        if (!other || other->getId() == user.getId())
            continue;

        const auto otherPosition = other->getObject2D().getPosition();
        const auto otherDimensions = other->getObject2D().getDimensions();
        const bool intersects =
                (newPosition.x - dimensions.x / 2 < otherPosition.x + otherDimensions.x / 2) &&
                (newPosition.x + dimensions.x / 2 > otherPosition.x - otherDimensions.x / 2) &&
                (newPosition.y - dimensions.y / 2 < otherPosition.y + otherDimensions.y / 2) &&
                (newPosition.y + dimensions.y / 2 > otherPosition.y - otherDimensions.y / 2);
        if (intersects)
            return CommandResult::success;
    }

    user.getObject2D().setPosition(newPosition);
    std::map<std::string, boost::json::value> params{
            {"x", boost::json::value(newPosition.x)},
            {"y", boost::json::value(newPosition.y)}};
    auto roomCommand = Command::createRoomCommand(user.getRoomId(), user, data, params,
                                                  args.getMessageId());
    Command::sendRoomCommand(roomCommand, user, args.getProtocol());
    return CommandResult::success;
}

} // namespace nexilis::server
