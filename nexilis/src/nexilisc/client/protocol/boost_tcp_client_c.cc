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
#include <nexilis/client/protocol/nxboost/tcp_client.hh>
#include <nexilisc/client/protocol/boost_tcp_client_c.h>

#include <chrono>
#include <memory>
#include <thread>

struct nexilis_BoostTCPClient
{
    nexilis::client::nxboost::TCPClient* client;
};

nexilis_BoostTCPClient* nexilis_boost_tcp_client_create(nexilis_ProtocolManagerC* manager, nexilis_ClientAPI* client_api)
{
    if (manager && manager->manager && client_api && client_api->api)
    {
        try
        {
            auto client = std::make_unique<nexilis::client::nxboost::TCPClient>(
                    manager->manager->createProtocol<nexilis::client::nxboost::TCPClient>(*client_api->api));
            return new nexilis_BoostTCPClient{client.release()};
        }
        catch (...)
        {
            return nullptr;
        }
    }
    return nullptr;
}

void nexilis_boost_tcp_client_destroy(nexilis_BoostTCPClient* client)
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

nexilis_ProtocolTypeC nexilis_boost_tcp_client_get_type(nexilis_BoostTCPClient* client)
{
    if (client && client->client)
    {
        return static_cast<nexilis_ProtocolTypeC>(client->client->getType());
    }
    return PROTOCOL_TYPE_UNKNOWN;
}

void nexilis_boost_tcp_client_start(nexilis_BoostTCPClient* client)
{
    if (client && client->client)
    {
        client->client->start();
    }
}

void nexilis_boost_tcp_client_stop(nexilis_BoostTCPClient* client)
{
    if (client && client->client)
    {
        client->client->stop();
    }
}

void nexilis_boost_tcp_client_send_message(nexilis_BoostTCPClient* client, const uint8_t message[], size_t message_size)
{
    if (client && client->client)
    {
        nexilis::nx_data data(message, message + message_size);
        client->client->sendMessage(data);
    }
}

void nexilis_boost_tcp_client_send_message_with_callback(nexilis_BoostTCPClient* client, const uint8_t message[], size_t message_size, void (*callback)(const uint8_t*, size_t))
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

bool nexilis_boost_tcp_client_is_connected(nexilis_BoostTCPClient* client)
{
    if (client && client->client)
    {
        return client->client->isConnected();
    }
    return false;
}

bool nexilis_start_client(nexilis_ClientAPI* client_api, nexilis_BoostTCPClient* tcp_client)
{
    if (!client_api || !client_api->api || !tcp_client || !tcp_client->client)
    {
        return false;
    }

    // 1. Start TCP client (connect + authenticate). start() is void and only
    //    logs on failure, so check the connection status explicitly: without
    //    this, a failed connection would fall through to the init wait below
    //    and block forever.
    tcp_client->client->start();

    if (tcp_client->client->getProtocolStatus() != nexilis::client::ProtocolStatus::connected)
    {
        return false;
    }

    // 2. Send clientId packet
    auto clientIdPacket = nexilis::client::Packet::Get::General::clientId(*client_api->api);
    tcp_client->client->sendMessage(clientIdPacket);

    // 3. Wait for initialization, bounded so a dead connection cannot hang
    //    the caller indefinitely. Also bail out early if the connection drops
    //    while we wait.
    constexpr auto initTimeout = std::chrono::seconds(5);
    const auto deadline = std::chrono::steady_clock::now() + initTimeout;
    while (!client_api->api->isInitialized())
    {
        if (tcp_client->client->getProtocolStatus() == nexilis::client::ProtocolStatus::error ||
            std::chrono::steady_clock::now() >= deadline)
        {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // 4. Send rooms info request
    auto roomsPacket = nexilis::client::Packet::Get::Info::rooms(*client_api->api);
    tcp_client->client->sendMessage(roomsPacket);

    return true;
}
