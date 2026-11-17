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

#include <nexilis/client/protocol/nxboost/udp_client.hh>
#include <nexilis/logger/log.hh>
#include <nexilisc/client/protocol/boost_udp_client_c.h>

#include <exception>
#include <functional>

struct nexilis_BoostUDPClient
{
    nexilis::client::nxboost::UDPClient* client;
};

nexilis_BoostUDPClient* nexilis_boost_udp_client_create(nexilis_ProtocolManagerC* manager, nexilis_ClientAPI* client_api)
{
    if (manager && manager->manager && client_api && client_api->api)
    {
        auto client = new nexilis_BoostUDPClient();
        client->client = new nexilis::client::nxboost::UDPClient(
                manager->manager->createProtocol<nexilis::client::nxboost::UDPClient>(*client_api->api));
        return client;
    }
    return nullptr;
}

void nexilis_boost_udp_client_destroy(nexilis_BoostUDPClient* client)
{
    if (client)
    {
        if (client->client)
        {
            delete client->client;
        }
        delete client;
    }
}

nexilis_ProtocolTypeC nexilis_boost_udp_client_get_type(nexilis_BoostUDPClient* client)
{
    if (client && client->client)
    {
        return static_cast<nexilis_ProtocolTypeC>(client->client->getType());
    }
    return PROTOCOL_TYPE_UNKNOWN;
}

void nexilis_boost_udp_client_start(nexilis_BoostUDPClient* client)
{
    if (client && client->client)
    {
        try
        {
            client->client->start();
        }
        catch (const std::exception& error)
        {
            nexilis::Log::error(error.what());
        }
    }
}

void nexilis_boost_udp_client_stop(nexilis_BoostUDPClient* client)
{
    if (client && client->client)
    {
        client->client->stop();
    }
}

void nexilis_boost_udp_client_send_message(nexilis_BoostUDPClient* client, const uint8_t message[], size_t message_size)
{
    if (client && client->client)
    {
        nexilis::nx_data data(message, message + message_size);
        client->client->sendMessage(data);
    }
}

void nexilis_boost_udp_client_send_message_with_callback(nexilis_BoostUDPClient* client, const uint8_t message[], size_t message_size, void (*callback)(const uint8_t*, size_t))
{
    if (!client || !client->client)
    {
        return;
    }

    nexilis::nx_data data(message, message + message_size);

    std::function<void()> cb;
    if (callback)
    {
        // Capture the data by value.
        cb = [callback, data]()
        {
            callback(data.data(), data.size());
        };
    }
    else
    {
        cb = []() {};
    }

    client->client->sendMessage(data, cb);
}

bool nexilis_boost_udp_client_is_connected(nexilis_BoostUDPClient* client)
{
    if (client && client->client)
    {
        return client->client->isConnected();
    }
    return false;
}
