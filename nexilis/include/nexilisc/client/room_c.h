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

#ifndef NEXILISC_CLIENT_ROOM_H_C
#define NEXILISC_CLIENT_ROOM_H_C

#include <stdint.h>
#include <stddef.h>

#include <nexilisc/room_data_c.h>

#include <nexilis/client/room.hh>

#ifdef __cplusplus
extern "C" {
#endif

struct nexilis_Room
{
    nexilis::client::Room* room;
    bool owned;
};

typedef struct nexilis_Communication nexilis_Communication;
typedef struct nexilis_ClientSession nexilis_ClientSession;

struct nexilis_RoomClients
{
    const std::vector<nexilis_ClientSession>* clients;
};

nexilis_Room* nexilis_room_create(nexilis_RoomData* room_data, nexilis_RoomClients* clients);
void nexilis_room_destroy(nexilis_Room* room);

uint64_t nexilis_room_get_id(nexilis_Room* room);
uint64_t nexilis_room_get_client_amount(nexilis_Room* room);

// Room metadata, mirrored from nexilis_room_data_get_* so that a client can
// build a room list from an existing room without a separate RoomData handle.
// nexilis_room_get_name returns a malloc'd string that the caller must free.
const char* nexilis_room_get_name(const nexilis_Room* room);
nexilis_RoomContext nexilis_room_get_context(const nexilis_Room* room);
uint32_t nexilis_room_get_max_size(const nexilis_Room* room);
uint64_t nexilis_room_get_creator_id(const nexilis_Room* room);

void nexilis_room_add_client(nexilis_Room* room, nexilis_ClientSession* client);
void nexilis_room_remove_client(nexilis_Room* room, uint64_t client_id);

nexilis_ClientSession** nexilis_room_get_clients(const nexilis_Room* room, size_t* num_clients);
void nexilis_room_free_client_array(nexilis_ClientSession** client_array, size_t num_clients);

void nexilis_room_add_message(nexilis_Room* room, nexilis_Communication* communication);
nexilis_Communication** nexilis_room_get_messages(const nexilis_Room* room, size_t* num_messages);
void nexilis_room_free_message_array(nexilis_Communication** message_array, size_t num_messages);
bool nexilis_room_contains_communication(const nexilis_Room* room, const nexilis_Communication* communication);
bool nexilis_room_contains_communication_by_id(const nexilis_Room* room, uint64_t communication_id);

nexilis_Communication* nexilis_communication_create(const char* payload, nexilis_ClientSession* sender);
void nexilis_communication_destroy(nexilis_Communication* communication);
const char* nexilis_communication_get_payload(const nexilis_Communication* communication);
const nexilis_ClientSession* nexilis_communication_get_client(const nexilis_Communication* communication);
    uint64_t nexilis_communication_get_id(const nexilis_Communication* communication);
    uint64_t nexilis_communication_get_sender_id(const nexilis_Communication* communication);
bool nexilis_communication_was_replayed(const nexilis_Communication* communication);

#ifdef __cplusplus
}
#endif

#endif
