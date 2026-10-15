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

#ifndef NEXILISC_PROTOCOL_MANAGER_C_H
#define NEXILISC_PROTOCOL_MANAGER_C_H

#include <nexilisc/protocol_type_c.h>
#include <nexilisc/protocol_status_c.h>

#include <nexilis/protocol_manager.hh>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct nexilis_ProtocolManagerC
{
    nexilis::ProtocolManager* manager;
};

typedef struct nexilis_ProtocolDataC nexilis_ProtocolDataC;

// ProtocolManager
nexilis_ProtocolManagerC* nexilis_protocol_manager_create();
void nexilis_protocol_manager_destroy(nexilis_ProtocolManagerC* manager);

uint16_t nexilis_get_boost_tcp_port();
void nexilis_set_boost_tcp_port(uint16_t port);
uint16_t nexilis_get_boost_udp_port();
void nexilis_set_boost_udp_port(uint16_t port);
uint16_t nexilis_get_boost_tcp_session_port();
void nexilis_set_boost_tcp_session_port(uint16_t port);

// ProtocolData
nexilis_ProtocolDataC* nexilis_protocol_data_create(nexilis_ProtocolTypeC type);
void nexilis_protocol_data_destroy(nexilis_ProtocolDataC* data);
nexilis_ProtocolTypeC nexilis_protocol_data_get_type(const nexilis_ProtocolDataC* data);
size_t nexilis_protocol_data_get_id(const nexilis_ProtocolDataC* data);

#ifdef __cplusplus
}
#endif

#endif
