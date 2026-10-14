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

#include <nexilis/client/packet.hh>
#include <nexilis/command_type.hh>
#include <nexilis/crypto.hh>
#include <nexilis/logger/log.hh>
#include <nexilis/movement_type.hh>
#include <nexilis/room_command_type.hh>
#include <nexilis/types/vector2.hh>
#include <nexilis/util.hh>

namespace nexilis::client
{

void _Packet::emplace(nx_data& originalData, const nx_data& newData)
{
    originalData.reserve(originalData.size() + newData.size());
    std::copy(newData.begin(), newData.end(), std::back_inserter(originalData));
}

nx_data _Packet::clientIdentification(ClientAPI& api)
{
    uint64_t clientId = api.getClientId();
    if (clientId == 0)
    {
        Log::error("Error creating new message id");
        return {};
    }

    auto clientIdVector = Util::convertToByteVector(clientId);

    assert(!clientIdVector.empty());
    assert(clientIdVector.size() == 8);
    assert(Util::convertToType<uint64_t>(clientIdVector) != 0);

    auto messageIdVector = Util::convertToByteVector(api.getNewMessageId());
    assert(messageIdVector.size() == 8);

    clientIdVector.reserve(clientIdVector.size() + messageIdVector.size());
    std::copy(messageIdVector.begin(), messageIdVector.end(), std::back_inserter(clientIdVector));

    return clientIdVector;
}

nx_data ClientImpl::set_general_clientId(ClientAPI& api, uint64_t newId)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::setting));
    id.emplace_back(0);
    id.emplace_back(0);

    auto id_vector = Util::convertToByteVector(newId);
    id.reserve(id.size() + id_vector.size());
    std::copy(id_vector.begin(), id_vector.end(), std::back_inserter(id));
    return id;
}

// Set: General
nx_data ClientImpl::set_general_username(ClientAPI& api, const std::string& name)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::setting));
    id.emplace_back(0);
    id.emplace_back(1);

    auto nameVector = Util::convertToByteVector(name.c_str(), name.size());
    _Packet::emplace(id, nameVector);
    return id;
}

// Set: Protocol::BoostTCP
nx_data set_protocol_boosttcp_port(ClientAPI& api, uint16_t port)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::setting));
    id.emplace_back(1);
    id.emplace_back(0);

    auto portVector = Util::convertToByteVector(port);
    _Packet::emplace(id, portVector);
    return id;
}

// Get: General
nx_data ClientImpl::get_general_clientId(ClientAPI& api)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::getting));
    id.emplace_back(0);
    id.emplace_back(0);
    return id;
}

nx_data ClientImpl::get_general_roomId(ClientAPI& api, uint64_t client_id)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::getting));
    id.emplace_back(0);
    id.emplace_back(2);

    auto clientBytes = Util::convertToByteVector(client_id);
    _Packet::emplace(id, clientBytes);
    return id;
}

// Get: Info
nx_data ClientImpl::get_info_general(ClientAPI& api)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::getting));
    id.emplace_back(1);
    id.emplace_back(0);
    return id;
}

nx_data ClientImpl::get_info_clients(ClientAPI& api)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::getting));
    id.emplace_back(1);
    id.emplace_back(1);
    return id;
}

nx_data ClientImpl::get_info_rooms(ClientAPI& api)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::getting));
    id.emplace_back(1);
    id.emplace_back(2);
    return id;
}

// Helper lambda to emplace any number of serializable arguments
static auto emplaceAll = [](nx_data& dest, const auto&... args)
{
    (_Packet::emplace(dest, Util::convertToByteVector(args)), ...);
};

// Room::Management
nx_data ClientImpl::room_management_join(ClientAPI& api, uint64_t roomId)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::management));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Management::join));
    emplaceAll(id, roomId);
    return id;
}

nx_data ClientImpl::room_management_leave(ClientAPI& api)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::management));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Management::leave));
    return id;
}

nx_data ClientImpl::room_management_create(ClientAPI& api, const std::string& roomName, RoomData::Context ctx)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::management));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Management::create));
    id.emplace_back(static_cast<uint8_t>(ctx));
    emplaceAll(id, roomName);
    return id;
}

nx_data ClientImpl::room_management_remove(ClientAPI& api, uint64_t roomId)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::management));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Management::remove));
    emplaceAll(id, roomId);
    return id;
}

nx_data ClientImpl::room_management_setOverlap(ClientAPI& api, bool allowed)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::management));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Management::set_overlap));
    id.emplace_back(allowed ? 1 : 0);
    return id;
}

// Room::Communicate
nx_data ClientImpl::room_communicate_broadcast(ClientAPI& api, const std::string& message)
{
    const auto useEncryption = api.isMessageEncryptionEnabled() && !api.getClientPassword().empty();
    const auto subcommand = useEncryption ? RoomCommandType::Communication::broadcast_encrypted
                                          : RoomCommandType::Communication::broadcast;

    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::communication));
    id.emplace_back(static_cast<uint8_t>(subcommand));

    std::string wireMessage = message;
    if (useEncryption)
    {
        const auto key = crypto::deriveMessageKey(api.getClientPassword(), api.clientRoomId());
        std::string sealed;
        if (key.empty() || !crypto::encrypt(key, message, sealed))
        {
            Log::error("Failed to encrypt message, falling back to plaintext broadcast");
        }
        else
        {
            wireMessage = crypto::toBase64(sealed);
        }
    }
    emplaceAll(id, wireMessage);
    return id;
}

nx_data ClientImpl::room_communicate_othercast(ClientAPI& api, const std::string& message)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::communication));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Communication::othercast));
    emplaceAll(id, message);
    return id;
}

nx_data ClientImpl::room_communicate_unicast(ClientAPI& api, uint64_t userId, const std::string& message)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::communication));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Communication::unicast));
    emplaceAll(id, userId, message);
    return id;
}

// Room::Player2D
nx_data ClientImpl::room_player2d_position(ClientAPI& api, Vector2f position)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::player_2D));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::PlayerType::position));
    emplaceAll(id, position);
    return id;
}

nx_data ClientImpl::room_player2d_dimension(ClientAPI& api, Vector2f dimensions)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::player_2D));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::PlayerType::dimension));
    emplaceAll(id, dimensions);
    return id;
}

nx_data ClientImpl::room_player2d_movement(ClientAPI& api, Vector2f movement, float deltatime)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::player_2D));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::PlayerType::movement));
    emplaceAll(id, movement, deltatime);
    return id;
}

nx_data ClientImpl::room_player2d_shoot(ClientAPI& api, Vector2f direction)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::player_2D));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::PlayerType::shoot));
    emplaceAll(id, direction);
    return id;
}

// Room::Object2D
nx_data ClientImpl::room_object2d_create(ClientAPI& api, Vector2f position, Vector2f dimensions, const std::string& filePath)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::object_2D));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::ObjectType::create));
    emplaceAll(id, position, dimensions, filePath);
    return id;
}

nx_data ClientImpl::room_object2d_destroy(ClientAPI& api, uint64_t objectId)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::object_2D));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::ObjectType::destroy));
    emplaceAll(id, objectId);
    return id;
}

nx_data ClientImpl::room_object2d_move(ClientAPI& api, uint64_t objectId, Vector2f newPosition)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::object_2D));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::ObjectType::move));
    emplaceAll(id, objectId, newPosition);
    return id;
}

nx_data ClientImpl::room_object2d_createMoving(ClientAPI& api, Vector2f startingPosition, Vector2f dimensions,
                                               Vector2f movement, float deltaTime,
                                               MovementType movementType, const std::string& filepath)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::object_2D));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::ObjectType::create_moving));
    emplaceAll(id, startingPosition, dimensions, movement, deltaTime, movementType, filepath);
    return id;
}

// Room::Player3D
nx_data ClientImpl::room_player3d_position(ClientAPI& api, Vector3f position)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::player_3D));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::PlayerType::position));
    emplaceAll(id, position);
    return id;
}

nx_data ClientImpl::room_player3d_position(ClientAPI& api, Vector3f position, float yaw)
{
    auto id = room_player3d_position(api, position);
    nx_emplace(id, yaw);
    return id;
}

nx_data ClientImpl::room_player3d_dimension(ClientAPI& api, Vector3f dimensions)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::player_3D));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::PlayerType::dimension));
    emplaceAll(id, dimensions);
    return id;
}

nx_data ClientImpl::room_player3d_movement(ClientAPI& api, Vector3f movement, float deltatime)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::player_3D));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::PlayerType::movement));
    emplaceAll(id, movement, deltatime);
    return id;
}

nx_data ClientImpl::room_player3d_movement(ClientAPI& api, Vector3f movement, float deltatime, float yaw)
{
    auto id = room_player3d_movement(api, movement, deltatime);
    emplaceAll(id, yaw);
    return id;
}

nx_data ClientImpl::room_player3d_shoot(ClientAPI& api, uint64_t targetId, float damage)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::player_3D));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::PlayerType::shoot));
    emplaceAll(id, targetId, damage);
    return id;
}

nx_data ClientImpl::room_player3d_fire(ClientAPI& api, Vector3f direction, uint8_t slot)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::player_3D));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::PlayerType::shoot));
    emplaceAll(id, direction, slot, static_cast<uint8_t>(0));
    return id;
}

nx_data ClientImpl::room_player3d_reload(ClientAPI& api, uint8_t slot)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::player_3D));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::PlayerType::shoot));
    emplaceAll(id, Vector3f{}, slot, static_cast<uint8_t>(1));
    return id;
}

nx_data ClientImpl::room_player3d_set_team(ClientAPI& api, const std::string& team)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::player_3D));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::PlayerType::set_team));
    emplaceAll(id, team);
    return id;
}

nx_data ClientImpl::room_player3d_audio_event(ClientAPI& api, uint8_t soundType, Vector3f position)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::player_3D));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::PlayerType::audio_event));
    emplaceAll(id, soundType, position);
    return id;
}

nx_data ClientImpl::room_player3d_match_action(ClientAPI& api, uint8_t action)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::player_3D));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::PlayerType::match_action));
    id.emplace_back(action);
    return id;
}

nx_data ClientImpl::room_object3d_create(ClientAPI& api, Vector3f position, Vector3f dimensions, const std::string& filePath)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::object_3D));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::ObjectType::create));
    emplaceAll(id, position, dimensions, filePath);
    return id;
}

nx_data ClientImpl::room_object3d_destroy(ClientAPI& api, uint64_t objectId)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::object_3D));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::ObjectType::destroy));
    emplaceAll(id, objectId);
    return id;
}

nx_data ClientImpl::room_object3d_move(ClientAPI& api, uint64_t objectId, Vector3f newPosition)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::object_3D));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::ObjectType::move));
    emplaceAll(id, objectId, newPosition);
    return id;
}

nx_data ClientImpl::room_object3d_createmoving(ClientAPI& api, Vector3f startingPosition, Vector3f dimensions,
                                               Vector3f movement, float deltaTime,
                                               MovementType movementType, const std::string& filepath)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::object_3D));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::ObjectType::create_moving));
    emplaceAll(id, startingPosition, dimensions, movement, deltaTime, movementType, filepath);
    return id;
}

// Room::GameItem
nx_data ClientImpl::room_gameitem_create(ClientAPI& api, Vector3f position, Vector3f dimensions,
                                         const std::string& type, const std::string& status,
                                         const std::string& filepath)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::game_item));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::GameItemAction::create));
    emplaceAll(id, position, dimensions);
    std::string combined = type + '\0' + status + '\0' + filepath;
    emplaceAll(id, combined);
    return id;
}

nx_data ClientImpl::room_gameitem_update(ClientAPI& api, uint64_t itemId, const std::string& status)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::game_item));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::GameItemAction::update));
    emplaceAll(id, itemId, status);
    return id;
}

nx_data ClientImpl::room_gameitem_destroy(ClientAPI& api, uint64_t itemId)
{
    auto id = _Packet::clientIdentification(api);
    id.emplace_back(static_cast<uint8_t>(CommandType::room));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::game_item));
    id.emplace_back(static_cast<uint8_t>(RoomCommandType::GameItemAction::destroy));
    emplaceAll(id, itemId);
    return id;
}

} // namespace nexilis::client
