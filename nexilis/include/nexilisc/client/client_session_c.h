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

#ifndef NEXILISC_CLIENT_SESSION_C_H
#define NEXILISC_CLIENT_SESSION_C_H

#include <stdbool.h>
#include <stdint.h>

#include <nexilisc/client/client_api_c.h>
#include <nexilisc/types/vector3_c.h>

#include <nexilis/client/client_session.hh>

#ifdef __cplusplus
extern "C" {
#endif

struct nexilis_ClientSession
{
    nexilis::client::ClientSession* client;
};

nexilis_ClientSession* nexilis_client_session_create(uint64_t id, nexilis_ClientAPI* client_api);
void nexilis_client_session_destroy(nexilis_ClientSession* session);
// Release a wrapper returned by nexilis_client_api_get_client_from_room.
// Its ClientSession belongs to the ClientAPI and must not be deleted here.
void nexilis_client_session_release_borrowed(nexilis_ClientSession* session);
// Free a nexilis_ClientSession wrapper struct without destroying the
// underlying ClientSession it points to. Use this when a wrapper was
// allocated by the caller (e.g. from nexilis_room_get_clients) and the
// underlying ClientSession is owned by someone else.
void nexilis_client_session_free_wrapper(nexilis_ClientSession* session);

uint64_t nexilis_client_session_get_id(nexilis_ClientSession* session);

void nexilis_client_session_set_position_3D(nexilis_ClientSession* client, float x, float y, float z);
nexilis_Vector3f* nexilis_client_session_get_position_3D(nexilis_ClientSession* client);
bool nexilis_client_session_get_position_3D_values(nexilis_ClientSession* client, float* x, float* y, float* z);

nexilis_ClientSession* nexilis_client_session_move(nexilis_ClientSession* other);
void nexilis_client_session_move_assign(nexilis_ClientSession* dest, nexilis_ClientSession* src);

bool nexilis_client_session_equal(const nexilis_ClientSession* lhs, const nexilis_ClientSession* rhs);
bool nexilis_client_session_not_equal(const nexilis_ClientSession* lhs, const nexilis_ClientSession* rhs);

void nexilis_client_session_set_username(nexilis_ClientSession* session, const char* new_username);

#ifdef __cplusplus
}
#endif

#endif
