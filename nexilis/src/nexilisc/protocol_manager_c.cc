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

#include <nexilis/ports.hh>
#include <nexilis/protocol_manager.hh>
#include <nexilisc/protocol_manager_c.h>

struct nexilis_ProtocolDataC
{
    nexilis::ProtocolManager::ProtocolData* data;
};

nexilis_ProtocolManagerC* nexilis_protocol_manager_create()
{
    auto protocol_manager = new nexilis_ProtocolManagerC();
    protocol_manager->manager = new nexilis::ProtocolManager();
    return protocol_manager;
}

void nexilis_protocol_manager_destroy(nexilis_ProtocolManagerC* manager)
{
    if (manager)
    {
        if (manager->manager)
        {
            delete manager->manager;
            manager->manager = nullptr;
        }
        delete manager;
    }
}

uint16_t nexilis_get_boost_tcp_port()
{
    return nexilis::Ports::getBoostTCPPort();
}

void nexilis_set_boost_tcp_port(uint16_t port)
{
    nexilis::Ports::setBoostTCPPort(port);
}

uint16_t nexilis_get_boost_udp_port()
{
    return nexilis::Ports::getBoostUDPPort();
}

void nexilis_set_boost_udp_port(uint16_t port)
{
    nexilis::Ports::setBoostUDPPort(port);
}

uint16_t nexilis_get_boost_tcp_session_port()
{
    return nexilis::Ports::getBoostTCPSessionPort();
}

void nexilis_set_boost_tcp_session_port(uint16_t port)
{
    nexilis::Ports::setBoostTCPSessionPort(port);
}

nexilis_ProtocolDataC* nexilis_protocol_data_create(nexilis_ProtocolTypeC type)
{
    auto data = new nexilis_ProtocolDataC();
    data->data = new nexilis::ProtocolManager::ProtocolData(static_cast<nexilis::NXProtocol::Type>(type));
    return data;
}

void nexilis_protocol_data_destroy(nexilis_ProtocolDataC* data)
{
    if (data)
    {
        if (data->data)
        {
            delete data->data;
            data->data = nullptr;
        }
        delete data;
    }
}

nexilis_ProtocolTypeC nexilis_protocol_data_get_type(const nexilis_ProtocolDataC* data)
{
    if (data && data->data)
    {
        return static_cast<nexilis_ProtocolTypeC>(data->data->getType());
    }
    return PROTOCOL_TYPE_UNKNOWN;
}

size_t nexilis_protocol_data_get_id(const nexilis_ProtocolDataC* data)
{
    if (data && data->data)
    {
        return data->data->getId();
    }
    return 0;
}
