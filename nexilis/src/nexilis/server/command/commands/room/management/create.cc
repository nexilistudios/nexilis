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

#include <nexilis/room_data.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/command/commands.hh>
#include <nexilis/server/room.hh>
#include <nexilis/server/room_limits.hh>
#include <nexilis/server/room_storage.hh>

#include <cstddef>

namespace nexilis::server
{

CommandResult ServerImpl::room_management_create(const DefaultArgs& args)
{
    constexpr std::size_t kCommandBytes = 3;
    constexpr std::size_t kFixedPayloadBytes = 3; // context + uint16 capacity
    constexpr std::size_t kMaxRoomNameBytes = 64;
    const auto& request = args.getData();
    if (request.size() < kCommandBytes + kFixedPayloadBytes + 1 ||
        request.size() > kCommandBytes + kFixedPayloadBytes + kMaxRoomNameBytes)
        return CommandResult::invalid_input;
    auto payload = Util::removeAmountOfBytesFromVector(request, kCommandBytes);
    uint8_t context = payload[0];
    const uint32_t maxSize = Util::convertToType<uint16_t>(
            Util::removeAmountOfBytesFromVector(payload, 1));
    std::string roomName = Util::convertToString(Util::removeAmountOfBytesFromVector(payload, 3));

    const auto& roomLimits = RoomLimits::get();
    const auto creatorId = args.getUser().getId();

    if (context > static_cast<uint8_t>(RoomData::Context::_3D) ||
        maxSize == 0 || maxSize > 64 || roomName == " ")
    {
        Log::error("Invalid room context, capacity, or name");
        return CommandResult::invalid_input;
    }

    auto room_data = RoomData(creatorId, roomName, Util::getRandomUint64(), static_cast<RoomData::Context>(context), maxSize);
    auto newRoom = nexilis::server::Room(room_data);

    auto newRoomId = newRoom.getId();
    if (!RoomStorage::add(std::move(newRoom), roomLimits.getMaxRooms(),
                          roomLimits.getMaxRoomsPerClient()))
        return CommandResult::failure;

    std::map<std::string, boost::json::value> params{
            {"room_name", boost::json::value(roomName)},
            {"client_id", boost::json::value(args.getUser().getId())},
            {"action", boost::json::value("create")},
            {"room_id", boost::json::value(newRoomId)},
            {"room_context", boost::json::value(context)},
            {"max_size", boost::json::value(maxSize)}};

    auto data = Command::clientMessageData(CommandType::room, "management", args.getMessageId(), params);

    if (Command::sendMessageToClient(data, args.getUser(), args.getProtocol()))
    {
        return CommandResult::success;
    }
    else
    {
        return CommandResult::failed_room_send;
    }
}

} // namespace nexilis::server
