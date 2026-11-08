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

#include <nexilis/object/object_2d.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/command/commands.hh>
#include <nexilis/server/room_limits.hh>
#include <nexilis/server/room_storage.hh>
#include <nexilis/server/validation.hh>

namespace nexilis::server
{

CommandResult ServerImpl::room_object2d_create(const DefaultArgs& args)
{
    auto& user = args.getUser();
    auto room = RoomStorage::getRoomById(user.getRoomId());
    if (!room || !room->isClientObjectCreationAllowed())
        return CommandResult::failure;

    auto data = args.getData();
    auto payload = Util::removeAmountOfBytesFromVector(data, 3);
    if (payload.size() < 16 || payload.size() > 16 + 256)
        return CommandResult::invalid_input;
    auto position = Util::vector2fFromFront(payload);
    auto dimensions = Util::vector2fFromFront(Util::removeAmountOfBytesFromVector(payload, 8));
    auto filepath = Util::convertToString(Util::removeAmountOfBytesFromVector(payload, 16));

    if (!validation::isValidPosition(position) ||
        !validation::isValidDimensions(dimensions) ||
        !validation::isValidOptionalAssetPath(filepath))
        return CommandResult::invalid_input;

    // Create server object.
    auto object = nexilis::Object2D(Util::getRandomUint64(), position, dimensions);
    object.setFilepath(filepath);
    uint64_t objectId = object.getId();
    Log::info("Created object with id: ", objectId);

    // Add to storage.
    if (!room->tryAddObject(std::move(object), RoomLimits::get().getMaxObjectsPerRoom()))
        return CommandResult::failure;

    std::map<std::string, boost::json::value> params{
            {"x", boost::json::value(position.x)},
            {"y", boost::json::value(position.y)},
            {"width", boost::json::value(dimensions.x)},
            {"height", boost::json::value(dimensions.y)},
            {"filepath", boost::json::value(filepath)},
            {"id", boost::json::value(objectId)}};

    auto roomCommand = Command::createRoomCommand(user.getRoomId(), user, data, params, args.getMessageId());
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
