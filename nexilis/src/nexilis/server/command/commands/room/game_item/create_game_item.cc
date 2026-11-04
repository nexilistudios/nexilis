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
#include <nexilis/server/gameplay_limits.hh>
#include <nexilis/server/room_storage.hh>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace nexilis::server
{

namespace
{

bool validLabel(std::string_view value, size_t maxLength)
{
    return !value.empty() && value.size() <= maxLength &&
           std::all_of(value.begin(), value.end(), [](unsigned char ch)
                       { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                                (ch >= '0' && ch <= '9') || ch == '_' || ch == '-'; });
}

bool validAssetPath(std::string_view path)
{
    if (path.empty() || path.size() > 256 || path.front() == '/')
        return false;
    size_t start = 0;
    while (start < path.size())
    {
        const size_t end = path.find('/', start);
        const auto segment = path.substr(start, end == std::string_view::npos ? end : end - start);
        if (segment.empty() || segment == "." || segment == ".." ||
            !std::all_of(segment.begin(), segment.end(), [](unsigned char ch)
                         { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                                  (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.'; }))
            return false;
        if (end == std::string_view::npos)
            return true;
        start = end + 1;
    }
    return false;
}

} // namespace

CommandResult ServerImpl::room_gameitem_create(const DefaultArgs& args)
{
    auto& user = args.getUser();
    auto room = RoomStorage::getRoomById(user.getRoomId());
    if (!room || !room->isClientGameItemCreationAllowed())
        return CommandResult::failure;
    if (room->getContext() != RoomData::Context::_3D)
        return CommandResult::invalid_input;
    auto data = args.getData();
    auto payload = Util::removeAmountOfBytesFromVector(data, 3);
    if (payload.size() < 29 || payload.size() > 24 + 64 + 1 + 32 + 1 + 256)
        return CommandResult::invalid_input;

    auto position = Util::vector3fFromFront(payload);
    payload = Util::removeAmountOfBytesFromVector(payload, 12);
    auto dimensions = Util::vector3fFromFront(payload);
    payload = Util::removeAmountOfBytesFromVector(payload, 12);

    const auto& limits = GameplayLimits::get();
    const float maxPosition = limits.getMaxHorizontalPosition();
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) ||
        !std::isfinite(dimensions.x) || !std::isfinite(dimensions.y) ||
        !std::isfinite(dimensions.z) || std::abs(position.x) > maxPosition ||
        std::abs(position.z) > maxPosition || position.y < limits.getMinHeight() ||
        position.y > limits.getMaxHeight() || dimensions.x <= 0.0f ||
        dimensions.y <= 0.0f || dimensions.z <= 0.0f ||
        dimensions.x > maxPosition || dimensions.y > maxPosition ||
        dimensions.z > maxPosition)
        return CommandResult::invalid_input;

    const std::string combined = Util::convertToString(payload);
    const size_t firstSeparator = combined.find('\0');
    if (firstSeparator == std::string::npos)
        return CommandResult::invalid_input;
    const size_t secondSeparator = combined.find('\0', firstSeparator + 1);
    if (secondSeparator == std::string::npos ||
        combined.find('\0', secondSeparator + 1) != std::string::npos)
        return CommandResult::invalid_input;
    const std::string item_type = combined.substr(0, firstSeparator);
    const std::string status = combined.substr(firstSeparator + 1, secondSeparator - firstSeparator - 1);
    const std::string filepath = combined.substr(secondSeparator + 1);
    if (!validLabel(item_type, 64) || !validLabel(status, 32) || !validAssetPath(filepath))
        return CommandResult::invalid_input;

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
