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
#include <nexilis/logger/file_log.hh>
#include <nexilis/util.hh>
#include <nexilisc/client/client_api_c.h>
#include <nexilisc/client/packet_c.h>

static nx_data_c convert_nx_data(const nexilis::nx_data& data)
{
    return nexilis_nx_data_create_from(data.data(), data.size());
}

static nexilis::Vector3f convert_vector3f(nexilis_Vector3f* vector)
{
    float x = vector->vec->x;
    float y = vector->vec->y;
    float z = vector->vec->z;
    nexilis::FileLog::debug("Converted position x:", x, " y:", y, " z:", z);
    return nexilis::Vector3f(x, y, z);
}

nx_data_c nexilis_packet_room_management_join(nexilis_ClientAPI* client_api, uint64_t room_id)
{
    return convert_nx_data(nexilis::client::Packet::Room::Management::join(*client_api->api, room_id));
}

nx_data_c nexilis_packet_room_management_leave(nexilis_ClientAPI* client_api)
{
    return convert_nx_data(nexilis::client::Packet::Room::Management::leave(*client_api->api));
}

nx_data_c nexilis_packet_room_management_create(nexilis_ClientAPI* client_api, const char* room_name, nexilis_RoomContext ctx)
{
    return convert_nx_data(nexilis::client::Packet::Room::Management::create(
            *client_api->api, std::string(room_name), nexilis::RoomData::Context(ctx)));
}

nx_data_c nexilis_packet_room_player3D_position(nexilis_ClientAPI* client_api, nexilis_Vector3f* position)
{
    if (!position || !position->vec)
    {
        nexilis::FileLog::error("Incorrect vector for nexilis_packet_room_player3D_position");
        return nx_data_c{};
    }

    return convert_nx_data(nexilis::client::Packet::Room::Player3D::position(*client_api->api, convert_vector3f(position)));
}

nx_data_c nexilis_packet_room_player3D_position_direct(nexilis_ClientAPI* client_api, float x, float y, float z)
{
    auto position = nexilis::client::Packet::Room::Player3D::position(*client_api->api, nexilis::Vector3f(x, y, z));
    return convert_nx_data(position);
}

nx_data_c nexilis_packet_room_player3D_dimension(nexilis_ClientAPI* client_api, nexilis_Vector3f* dimensions)
{
    return convert_nx_data(nexilis::client::Packet::Room::Player3D::dimension(*client_api->api, convert_vector3f(dimensions)));
}

nx_data_c nexilis_packet_room_player3D_dimension_direct(nexilis_ClientAPI* client_api, float x, float y, float z)
{
    return convert_nx_data(nexilis::client::Packet::Room::Player3D::dimension(*client_api->api, nexilis::Vector3f(x, y, z)));
}

nx_data_c nexilis_packet_room_player3D_movement(nexilis_ClientAPI* client_api, nexilis_Vector3f* movement, float deltaTime)
{
    return convert_nx_data(nexilis::client::Packet::Room::Player3D::movement(*client_api->api, convert_vector3f(movement), deltaTime));
}

nx_data_c nexilis_packet_room_player3D_movement_direct(nexilis_ClientAPI* client_api, float x, float y, float z, float deltatime)
{
    return convert_nx_data(nexilis::client::Packet::Room::Player3D::movement(*client_api->api, nexilis::Vector3f(x, y, z), deltatime));
}

nx_data_c nexilis_packet_room_player3D_set_team(nexilis_ClientAPI* client_api, const char* team)
{
    if (!client_api || !client_api->api || !team)
        return {};
    return convert_nx_data(nexilis::client::ClientImpl::room_player3d_set_team(*client_api->api, team));
}

nx_data_c nexilis_packet_room_player3D_shoot(nexilis_ClientAPI* client_api, uint64_t target_id, float damage)
{
    if (!client_api || !client_api->api)
        return {};
    return convert_nx_data(nexilis::client::ClientImpl::room_player3d_shoot(*client_api->api, target_id, damage));
}

nx_data_c nexilis_packet_room_player3D_fire(nexilis_ClientAPI* client_api, float x, float y, float z, uint8_t slot)
{
    if (!client_api || !client_api->api)
        return {};
    return convert_nx_data(nexilis::client::ClientImpl::room_player3d_fire(
            *client_api->api, nexilis::Vector3f(x, y, z), slot));
}

nx_data_c nexilis_packet_room_player3D_audio_event(nexilis_ClientAPI* client_api, uint8_t sound, float x, float y, float z)
{
    if (!client_api || !client_api->api)
        return {};
    return convert_nx_data(nexilis::client::ClientImpl::room_player3d_audio_event(
            *client_api->api, sound, nexilis::Vector3f(x, y, z)));
}

nx_data_c nexilis_packet_room_communicate_broadcast(nexilis_ClientAPI* client_api, const char* message)
{
    if (!client_api || !client_api->api || !message)
        return {};
    return convert_nx_data(nexilis::client::ClientImpl::room_communicate_broadcast(*client_api->api, message));
}

nx_data_c nexilis_packet_get_general_clientId(nexilis_ClientAPI* client_api)
{
    return convert_nx_data(nexilis::client::Packet::Get::General::clientId(*client_api->api));
}

nx_data_c nexilis_packet_get_info_general(nexilis_ClientAPI* client_api)
{
    return convert_nx_data(nexilis::client::Packet::Get::Info::general(*client_api->api));
}

nx_data_c nexilis_packet_get_info_clients(nexilis_ClientAPI* client_api)
{
    return convert_nx_data(nexilis::client::Packet::Get::Info::clients(*client_api->api));
}

nx_data_c nexilis_packet_get_info_rooms(nexilis_ClientAPI* client_api)
{
    return convert_nx_data(nexilis::client::Packet::Get::Info::rooms(*client_api->api));
}
