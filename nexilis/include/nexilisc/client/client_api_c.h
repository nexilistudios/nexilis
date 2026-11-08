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

#ifndef NEXILISC_CLIENT_API_C_H
#define NEXILISC_CLIENT_API_C_H

#include <nexilisc/client/client_config_c.h>
#include <nexilisc/client/rooms_collection.h>

#include <nexilis/client/client_api.hh>

#include <memory>

#ifdef __cplusplus
extern "C"
{
#endif

    struct nexilis_ClientAPI
    {
        nexilis::client::ClientAPI* api;
        std::shared_ptr<nexilis::client::ClientAPI> owner;
    };

    nexilis_ClientAPI* nexilis_client_api_create(nexilis_ClientConfigC* config);
    void nexilis_client_api_destroy(nexilis_ClientAPI* client_api);

    bool nexilis_client_api_is_inet_udp_ready(const nexilis_ClientAPI* client_api);
    bool nexilis_client_api_is_inet_tcp_ready(const nexilis_ClientAPI* client_api);
    bool nexilis_client_api_is_boost_tcp_ready(const nexilis_ClientAPI* client_api);
    bool nexilis_client_api_is_boost_udp_ready(const nexilis_ClientAPI* client_api);
    bool nexilis_client_api_is_unix_dgram_ready(const nexilis_ClientAPI* client_api);
    bool nexilis_client_api_is_unix_stream_ready(const nexilis_ClientAPI* client_api);

    bool nexilis_client_api_is_initialized(const nexilis_ClientAPI* client_api);
    void nexilis_client_api_wait_until_inet_udp_ready(nexilis_ClientAPI* client_api);
    void nexilis_client_api_wait_until_inet_tcp_ready(nexilis_ClientAPI* client_api);
    void nexilis_client_api_wait_until_boost_tcp_ready(nexilis_ClientAPI* client_api);
    void nexilis_client_api_wait_until_boost_udp_ready(nexilis_ClientAPI* client_api);
    void nexilis_client_api_wait_until_unix_dgram_ready(nexilis_ClientAPI* client_api);
    void nexilis_client_api_wait_until_unix_stream_ready(nexilis_ClientAPI* client_api);

    const char* nexilis_client_api_get_inet_udp_server_address(const nexilis_ClientAPI* client_api);
    const char* nexilis_client_api_get_inet_tcp_server_address(const nexilis_ClientAPI* client_api);
    const char* nexilis_client_api_get_boost_tcp_server_address(const nexilis_ClientAPI* client_api);
    const char* nexilis_client_api_get_boost_udp_server_address(const nexilis_ClientAPI* client_api);
    const char* nexilis_client_api_get_unix_dgram_path(const nexilis_ClientAPI* client_api);
    const char* nexilis_client_api_get_unix_stream_path(const nexilis_ClientAPI* client_api);

    uint64_t nexilis_client_api_get_client_id(const nexilis_ClientAPI* client_api);
    const char* nexilis_client_api_get_client_username(const nexilis_ClientAPI* client_api, uint64_t client_id);
    const char* nexilis_client_api_get_client_password(const nexilis_ClientAPI* client_api);

    nexilis_RoomsCollection* nexilis_client_api_get_active_rooms(const nexilis_ClientAPI* client_api);
    size_t nexilis_client_api_rooms_count(const nexilis_ClientAPI* client_api);

    // Get id of the room where the client is currently in.
    size_t nexilis_client_api_client_room_id(const nexilis_ClientAPI* client_api);
    // Return an owned snapshot copied under the room lock. Free it with
    // nexilis_room_destroy. The snapshot stays valid if the client leaves the
    // room or the API's internal room state changes.
    nexilis_Room* nexilis_client_api_get_room(const nexilis_ClientAPI* client_api, uint64_t room_id);
    // Return an owned snapshot copied under the room lock. Free it with
    // nexilis_client_session_destroy; nexilis_client_session_release_borrowed
    // is retained as a legacy alias.
    nexilis_ClientSession* nexilis_client_api_get_client_from_room(const nexilis_ClientAPI* client_api, uint64_t client_id);

    // UTF-8 JSON containing pending damage, respawn and leaderboard events.
    // Free the returned buffer with nexilis_client_api_free_events.
    char* nexilis_client_api_drain_game_events(nexilis_ClientAPI* client_api, size_t chat_since);
    void nexilis_client_api_free_events(char* events);

#ifdef __cplusplus
}
#endif

#endif
