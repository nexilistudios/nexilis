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

#include <nexilis/movement_type.hh>
#include <nexilis/object/object_2d.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/command/commands.hh>
#include <nexilis/server/movement.hh>
#include <nexilis/server/room_limits.hh>
#include <nexilis/server/room_storage.hh>
#include <nexilis/server/validation.hh>

#include <cmath>

namespace nexilis::server
{

namespace
{
constexpr float kMaxClientMovementDelta = 0.25f;
}

CommandResult ServerImpl::room_object2d_createMoving(const DefaultArgs& args)
{
    auto& user = args.getUser();
    auto room = RoomStorage::getRoomById(user.getRoomId());
    if (!room || !room->isClientObjectCreationAllowed())
        return CommandResult::failure;

    auto data = args.getData();

    // Get messagedata.
    auto payload = Util::removeAmountOfBytesFromVector(data, 3);
    if (payload.size() < 29 || payload.size() > 29 + 256)
        return CommandResult::invalid_input;
    auto startingPosition = Util::vector2fFromFront(payload);
    auto dimensions = Util::vector2fFromFront(Util::removeAmountOfBytesFromVector(payload, 8));
    auto movement = Util::vector2fFromFront(Util::removeAmountOfBytesFromVector(payload, 16));
    auto deltaTime = Util::floatFromFront(Util::removeAmountOfBytesFromVector(payload, 24));
    auto movementType = static_cast<MovementType>(payload[28]);
    auto filepath = Util::convertToString(Util::removeAmountOfBytesFromVector(payload, 29));

    if (!validation::isValidPosition(startingPosition) ||
        !validation::isValidDimensions(dimensions) ||
        !validation::isFinite(movement) || !std::isfinite(deltaTime) ||
        deltaTime <= 0.0f || deltaTime > kMaxClientMovementDelta ||
        (movementType != MovementType::eased && movementType != MovementType::linear) ||
        !validation::isValidOptionalAssetPath(filepath))
        return CommandResult::invalid_input;

    // Create new object.
    auto object = nexilis::Object2D(Util::getRandomUint64(), startingPosition, dimensions);
    object.setFilepath(filepath);
    uint64_t objectId = object.getId();
    Log::info("Created object with id: ", objectId);

    // Add newly created object to storage.
    if (!room->tryAddObject(std::move(object), RoomLimits::get().getMaxObjectsPerRoom()))
        return CommandResult::failure;

    std::map<std::string, boost::json::value> params{
            {"createMovingType", boost::json::value("create")},
            {"x", boost::json::value(startingPosition.x)},
            {"y", boost::json::value(startingPosition.y)},
            {"width", boost::json::value(dimensions.x)},
            {"height", boost::json::value(dimensions.y)},
            {"filepath", boost::json::value(filepath)},
            {"id", boost::json::value(objectId)}};
    auto roomCommand = Command::createRoomCommand(user.getRoomId(), user, data, params, args.getMessageId());
    Command::sendRoomCommand(roomCommand, user, args.getProtocol());

    std::function<double(double, double)> movementFunction;
    switch (movementType)
    {
        case MovementType::eased:
            movementFunction = [](double progress, double totalDistance) -> double
            {
                return Movement::easing(progress, totalDistance);
            };
            break;
        case MovementType::linear:
            movementFunction = [](double progress, double totalDistance) -> double
            {
                return Movement::linear(progress, totalDistance);
            };
            break;
    }

    auto base_move = std::make_unique<Movement2D>(MovementData(objectId, deltaTime, data, args.getMessageId()), movement, movementFunction);
    Movement::object2D(std::move(base_move), user, args.getProtocol());
    return CommandResult::success;
}

} // namespace nexilis::server
