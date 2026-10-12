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
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/command/commands.hh>
#include <nexilis/server/room_storage.hh>

namespace nexilis::server
{

CommandResult ServerImpl::room_management_join(const DefaultArgs& args)
{
    Log::debug("Commands::Room::Management::join", args);
    auto user_id = args.getUser().getId();

    auto payload = Util::removeAmountOfBytesFromVector(args.getData(), 3);
    uint64_t roomId = Util::convertToType<uint64_t>(payload);
    auto* room = RoomStorage::getRoomById(roomId);

    if (!room)
    {
        Log::error("Cannot find room with specified id!");
        return CommandResult::invalid_input;
    }

    if (RoomStorage::getRoomById(roomId)->contains(user_id))
    {
        Log::error("Cannot join room where the client already is!");
        return CommandResult::error;
    }

    room->joinRoom(user_id);

    args.getUser().setRoomId(room->getId());
    assert(ClientStorage::getClientById(user_id)->getRoomId() == room->getId());
    assert(RoomStorage::getRoomById(roomId)->contains(user_id));
    assert(args.getUser().getRoomId() == roomId);

    auto roomCommand = Command::createRoomCommand(roomId, args.getUser(), args.getData(),
                                                  {{"username", boost::json::value(args.getUser().getUsername())}}, args.getMessageId());
    Command::sendRoomCommand(roomCommand, args.getUser(), args.getProtocol());

    // Send existing players to the joining client
    auto& user = args.getUser();
    auto& protocol = args.getProtocol();
    for (auto& existingClientId : room->getClients())
    {
        if (existingClientId == user_id)
            continue;
        auto* existingUser = ClientStorage::getClientById(existingClientId);
        if (!existingUser)
            continue;

        // Send "join" to create a ClientSession on the joining client
        std::map<std::string, boost::json::value> joinParams{{"username", boost::json::value(existingUser->getUsername())}};
        auto joinMsg = Command::createRoomCommand(roomId, *existingUser, args.getData(), joinParams, args.getMessageId());
        Command::sendMessageToClient(joinMsg, user, protocol);

        // Send current position
        auto pos = existingUser->getObject3D().getPosition();
        std::map<std::string, boost::json::value> posParams{
                {"action", boost::json::value("position")},
                {"room_id", boost::json::value(roomId)},
                {"client_id", boost::json::value(existingClientId)},
                {"x", boost::json::value(pos.x)},
                {"y", boost::json::value(pos.y)},
                {"z", boost::json::value(pos.z)},
                {"yaw", boost::json::value(existingUser->getObject3D().getYaw())}};
        auto posMsg = Command::clientMessageData(CommandType::room, "player_3D", 0, posParams);
        Command::sendMessageToClient(posMsg, user, protocol);

        // Send current dimensions
        auto dim = existingUser->getObject3D().getDimensions();
        std::map<std::string, boost::json::value> dimParams{
                {"action", boost::json::value("dimensions")},
                {"room_id", boost::json::value(roomId)},
                {"client_id", boost::json::value(existingClientId)},
                {"x", boost::json::value(dim.x)},
                {"y", boost::json::value(dim.y)},
                {"z", boost::json::value(dim.z)}};
        auto dimMsg = Command::clientMessageData(CommandType::room, "player_3D", 0, dimParams);
        Command::sendMessageToClient(dimMsg, user, protocol);
    }

    // Send existing game items to the joining client
    for (auto& item : room->getGameItems())
    {
        auto pos = item.getPosition();
        auto dim = item.getDimensions();
        std::map<std::string, boost::json::value> itemParams{
                {"action", boost::json::value("create")},
                {"room_id", boost::json::value(roomId)},
                {"client_id", boost::json::value(user_id)},
                {"id", boost::json::value(item.getId())},
                {"x", boost::json::value(pos.x)},
                {"y", boost::json::value(pos.y)},
                {"z", boost::json::value(pos.z)},
                {"w", boost::json::value(dim.x)},
                {"h", boost::json::value(dim.y)},
                {"d", boost::json::value(dim.z)},
                {"item_type", boost::json::value(item.getType())},
                {"status", boost::json::value(item.getStatus())},
                {"filepath", boost::json::value(item.getFilepath())}};
        auto itemMsg = Command::clientMessageData(CommandType::room, "game_item", 0, itemParams);
        Command::sendMessageToClient(itemMsg, user, protocol);
    }

    // Send the room's broadcast history to the joining client
    for (const auto& broadcast : room->getBroadcasts())
    {
        std::map<std::string, boost::json::value> broadcastParams{
                {"action", boost::json::value("broadcast")},
                {"room_id", boost::json::value(roomId)},
                {"client_id", boost::json::value(broadcast.clientId)},
                {"message", boost::json::value(broadcast.message)}};
        auto broadcastMsg = Command::clientMessageData(CommandType::room, "communication", 0, broadcastParams);
        Command::sendMessageToClient(broadcastMsg, user, protocol);
    }

    return CommandResult::success;
}

} // namespace nexilis::server
