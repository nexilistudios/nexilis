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

#ifndef NEXILISC_CLIENT_PACKET_C_H
#define NEXILISC_CLIENT_PACKET_C_H

#include <nexilisc/nx_data_c.h>
#include <nexilisc/room_context_c.h>

#include <nexilisc/types/vector3_c.h>

struct nexilis_ClientAPI;

#ifdef __cplusplus
extern "C"
{
#endif

    // Info messages
    nx_data_c nexilis_packet_get_general_clientId(nexilis_ClientAPI* client_api);
    nx_data_c nexilis_packet_get_info_general(nexilis_ClientAPI* client_api);
    nx_data_c nexilis_packet_get_info_clients(nexilis_ClientAPI* client_api);
    nx_data_c nexilis_packet_get_info_rooms(nexilis_ClientAPI* client_api);

    // Room management
    nx_data_c nexilis_packet_room_management_join(nexilis_ClientAPI* client_api, uint64_t room_id);
    nx_data_c nexilis_packet_room_management_leave(nexilis_ClientAPI* client_api);
    nx_data_c nexilis_packet_room_management_create(nexilis_ClientAPI* client_api, const char* room_name, nexilis_RoomContext ctx = ROOM_CONTEXT_3D);

    // Room player3D
    nx_data_c nexilis_packet_room_player3D_position(nexilis_ClientAPI* client_api, nexilis_Vector3f* position);
    nx_data_c nexilis_packet_room_player3D_position_direct(nexilis_ClientAPI* client_api, float x, float y, float z);
    nx_data_c nexilis_packet_room_player3D_dimension(nexilis_ClientAPI* client_api, nexilis_Vector3f* dimensions);
    nx_data_c nexilis_packet_room_player3D_dimension_direct(nexilis_ClientAPI* client_api, float x, float y, float z);
    nx_data_c nexilis_packet_room_player3D_movement(nexilis_ClientAPI* client_api, nexilis_Vector3f* movement, float deltaTime);
    nx_data_c nexilis_packet_room_player3D_movement_direct(nexilis_ClientAPI* client_api, float x, float y, float z, float deltaTime);
    nx_data_c nexilis_packet_room_player3D_set_team(nexilis_ClientAPI* client_api, const char* team);
    /* Deprecated wire format: servers reject client-selected hits. */
    nx_data_c nexilis_packet_room_player3D_shoot(nexilis_ClientAPI* client_api, uint64_t target_id, float damage);
    nx_data_c nexilis_packet_room_player3D_fire(nexilis_ClientAPI* client_api, float x, float y, float z, uint8_t slot);
    nx_data_c nexilis_packet_room_player3D_audio_event(nexilis_ClientAPI* client_api, uint8_t sound, float x, float y, float z);
    nx_data_c nexilis_packet_room_communicate_broadcast(nexilis_ClientAPI* client_api, const char* message);

#ifdef __cplusplus
}
#endif

#endif
