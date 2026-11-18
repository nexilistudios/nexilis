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

#ifndef NEXILISC_CLIENT_CONFIG_C_H
#define NEXILISC_CLIENT_CONFIG_C_H

#include <nexilis/client/client_api.hh>

#ifdef __cplusplus
extern "C" {
#endif

struct nexilis_ClientConfigC
{
    nexilis::client::ClientConfig* data;
};

nexilis_ClientConfigC* nexilis_client_config_create();
void nexilis_client_config_destroy(nexilis_ClientConfigC* config);

void nexilis_client_config_set_password(nexilis_ClientConfigC* config, const char* password);
void nexilis_client_config_set_account_credentials(nexilis_ClientConfigC* config,
                                                    const char* account_id, const char* credential);
const char* nexilis_client_config_get_password(const nexilis_ClientConfigC* config);

void nexilis_client_config_set_mode(nexilis_ClientConfigC* config, int mode);

void nexilis_client_config_set_message_encryption(nexilis_ClientConfigC* config, int enabled);
int nexilis_client_config_get_message_encryption(const nexilis_ClientConfigC* config);

void nexilis_client_config_set_tls(nexilis_ClientConfigC* config, int enabled);
int nexilis_client_config_get_tls(const nexilis_ClientConfigC* config);
void nexilis_client_config_set_server_public_key_sha256(nexilis_ClientConfigC* config, const char* hex);
const char* nexilis_client_config_get_server_public_key_sha256(const nexilis_ClientConfigC* config);

void nexilis_client_config_set_inet_udp(nexilis_ClientConfigC* config, const char* server_address);
const char* nexilis_client_config_get_inet_udp_server_address(const nexilis_ClientConfigC* config);

void nexilis_client_config_set_inet_tcp(nexilis_ClientConfigC* config, const char* server_address);
const char* nexilis_client_config_get_inet_tcp_server_address(const nexilis_ClientConfigC* config);

void nexilis_client_config_set_boost_tcp_address(nexilis_ClientConfigC* config, const char* server_address);
const char* nexilis_client_config_get_boost_tcp_server_address(const nexilis_ClientConfigC* config);

void nexilis_client_config_set_boost_udp_address(nexilis_ClientConfigC* config, const char* server_address);
const char* nexilis_client_config_get_boost_udp_server_address(const nexilis_ClientConfigC* config);

void nexilis_client_config_set_unix_dgram_server_path(nexilis_ClientConfigC* config, const char* socket_path);
const char* nexilis_client_config_get_unix_dgram_server_path(const nexilis_ClientConfigC* config);

void nexilis_client_config_set_unix_stream_server_path(nexilis_ClientConfigC* config, const char* socket_path);
const char* nexilis_client_config_get_unix_stream_server_path(const nexilis_ClientConfigC* config);

#ifdef __cplusplus
}
#endif


#endif
