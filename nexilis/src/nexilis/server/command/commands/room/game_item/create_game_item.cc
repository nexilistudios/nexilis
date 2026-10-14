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

#include <nexilis/object/game_item.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/command/commands.hh>
#include <nexilis/server/room_storage.hh>

#include <sstream>

namespace nexilis::server
{

CommandResult ServerImpl::room_gameitem_create(const DefaultArgs& args)
{
    auto& user = args.getUser();
    auto room = RoomStorage::getRoomById(user.getRoomId());
    if (!room || !room->isClientGameItemCreationAllowed())
        return CommandResult::failure;
    auto data = args.getData();
    auto payload = Util::removeAmountOfBytesFromVector(data, 3);

    auto position = Util::vector3fFromFront(payload);
    payload = Util::removeAmountOfBytesFromVector(payload, 12);
    auto dimensions = Util::vector3fFromFront(payload);
    payload = Util::removeAmountOfBytesFromVector(payload, 12);

    auto combined = Util::convertToString(payload);
    std::vector<std::string> parts;
    std::stringstream ss(combined);
    std::string part;
    while (std::getline(ss, part, '\0'))
    {
        parts.push_back(part);
    }

    std::string item_type = parts.size() > 0 ? parts[0] : "";
    std::string status = parts.size() > 1 ? parts[1] : "";
    std::string filepath = parts.size() > 2 ? parts[2] : "";

    auto item = GameItem(Util::getRandomUint64(), item_type, position, dimensions, status, filepath);
    uint64_t itemId = item.getId();
    Log::info("Created game item with id: ", itemId, " type: ", item_type);

    room->addGameItem(std::move(item));

    std::map<std::string, boost::json::value> params{
            {"id", boost::json::value(itemId)},
            {"x", boost::json::value(position.x)},
            {"y", boost::json::value(position.y)},
            {"z", boost::json::value(position.z)},
            {"w", boost::json::value(dimensions.x)},
            {"h", boost::json::value(dimensions.y)},
            {"d", boost::json::value(dimensions.z)},
            {"item_type", boost::json::value(item_type)},
            {"status", boost::json::value(status)},
            {"filepath", boost::json::value(filepath)}};

    auto roomCommand = Command::createRoomCommand(user.getRoomId(), user, data, params, args.getMessageId());
    if (Command::sendRoomCommand(roomCommand, user, args.getProtocol()))
        return CommandResult::success;
    else
        return CommandResult::failed_room_send;
}

} // namespace nexilis::server
