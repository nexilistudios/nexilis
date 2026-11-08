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

#include <nexilis/client/room.hh>
#include <nexilisc/client/client_api_c.h>
#include <nexilisc/client/client_session_c.h>
#include <nexilisc/client/room_c.h>

#include <cstdlib>
#include <cstring>

struct nexilis_Communication
{
    nexilis::client::Room::Communication* communication;
};

nexilis_Room* nexilis_room_create(nexilis_RoomData* room_data, nexilis_RoomClients* clients)
{
    std::vector<nexilis::client::ClientSession> clientSessions;

    if (clients && clients->clients)
    {
        clientSessions.reserve(clients->clients->size());

        for (const auto& client : *(clients->clients))
        {
            auto cpp_client = static_cast<nexilis::client::ClientSession*>(client.client);
            clientSessions.emplace_back(std::move(*cpp_client));
        }
    }

    auto room = new nexilis_Room;
    room->room = new nexilis::client::Room(*room_data->data, std::move(clientSessions));
    room->owned = true;
    return room;
}

void nexilis_room_destroy(nexilis_Room* room)
{
    if (room)
    {
        if (room->room && room->owned)
        {
            delete room->room;
            room->room = nullptr;
        }
        delete room;
    }
}

uint64_t nexilis_room_get_client_amount(nexilis_Room* room)
{
    if (room && room->room)
    {
        return room->room->getClients().size();
    }
    return 0;
}

uint64_t nexilis_room_get_id(nexilis_Room* room)
{
    if (room && room->room)
    {
        return room->room->getId();
    }
    return 0;
}

const char* nexilis_room_get_name(const nexilis_Room* room)
{
    if (room && room->room)
    {
        std::string name = room->room->getName();
        char* cstr = (char*)malloc(name.size() + 1);
        if (cstr)
        {
            std::strcpy(cstr, name.c_str());
        }
        return cstr;
    }
    return nullptr;
}

nexilis_RoomContext nexilis_room_get_context(const nexilis_Room* room)
{
    if (room && room->room)
    {
        return static_cast<nexilis_RoomContext>(room->room->getContext());
    }
    return ROOM_CONTEXT_UNDEFINED;
}

uint32_t nexilis_room_get_max_size(const nexilis_Room* room)
{
    if (room && room->room)
    {
        return room->room->getMaxSize();
    }
    return 0;
}

uint64_t nexilis_room_get_creator_id(const nexilis_Room* room)
{
    if (room && room->room)
    {
        return room->room->getCreatorId();
    }
    return 0;
}

void nexilis_room_add_client(nexilis_Room* room, nexilis_ClientSession* client)
{
    if (room && room->room && client && client->client)
    {
        room->room->addClient(std::move(*client->client));
    }
}

void nexilis_room_remove_client(nexilis_Room* room, uint64_t client_id)
{
    if (room && room->room)
    {
        room->room->removeClient(client_id);
    }
}

nexilis_ClientSession** nexilis_room_get_clients(const nexilis_Room* room, size_t* num_clients)
{
    if (!room || !room->room || !num_clients)
    {
        return nullptr;
    }

    const auto& clients = room->room->getClients();
    *num_clients = clients.size();

    if (*num_clients == 0)
    {
        return nullptr;
    }

    // Allocate array of pointers.
    nexilis_ClientSession** client_array = static_cast<nexilis_ClientSession**>(
            malloc(sizeof(nexilis_ClientSession*) * (*num_clients)));

    // Convert each client to C struct pointer.
    for (size_t i = 0; i < *num_clients; ++i)
    {
        client_array[i] = new nexilis_ClientSession();
        client_array[i]->client = const_cast<nexilis::client::ClientSession*>(&clients[i]);
    }
    return client_array;
}

void nexilis_room_free_client_array(nexilis_ClientSession** client_array, size_t num_clients)
{
    if (!client_array)
    {
        return;
    }

    for (size_t i = 0; i < num_clients; ++i)
    {
        if (client_array[i])
        {
            delete client_array[i];
        }
    }
    free(client_array);
}

void nexilis_room_add_message(nexilis_Room* room, nexilis_Communication* communication)
{
    if (room && room->room && communication && communication->communication)
    {
        room->room->addMessage(std::move(*communication->communication));
    }
}

nexilis_Communication** nexilis_room_get_messages(const nexilis_Room* room, size_t* num_messages)
{
    if (!room || !room->room || !num_messages)
    {
        return nullptr;
    }

    const auto& messages = room->room->getMessages();
    *num_messages = messages.size();

    if (*num_messages == 0)
    {
        return nullptr;
    }

    // Allocate array of pointers.
    nexilis_Communication** message_array = static_cast<nexilis_Communication**>(
            malloc(sizeof(nexilis_Communication*) * (*num_messages)));

    // Copy each message to a new C struct pointer.
    for (size_t i = 0; i < *num_messages; ++i)
    {
        message_array[i] = new nexilis_Communication();
        message_array[i]->communication = new nexilis::client::Room::Communication(messages[i]);
    }
    return message_array;
}

void nexilis_room_free_message_array(nexilis_Communication** message_array, size_t num_messages)
{
    if (!message_array)
    {
        return;
    }

    for (size_t i = 0; i < num_messages; ++i)
    {
        if (message_array[i])
        {
            if (message_array[i]->communication)
            {
                delete message_array[i]->communication;
            }
            delete message_array[i];
        }
    }
    free(message_array);
}

bool nexilis_room_contains_communication(const nexilis_Room* room, const nexilis_Communication* communication)
{
    if (room && room->room && communication && communication->communication)
    {
        return room->room->containsCommunication(*communication->communication);
    }
    assert(!"Undefined room");
    return false;
}

bool nexilis_room_contains_communication_by_id(const nexilis_Room* room, uint64_t communication_id)
{
    if (room && room->room)
    {
        return room->room->containsCommunication(communication_id);
    }
    assert(!"Undefined room");
    return false;
}

nexilis_Communication* nexilis_communication_create(const char* payload, nexilis_ClientSession* sender)
{
    if (sender && sender->client && payload)
    {
        auto communication = new nexilis_Communication();
        auto cpp_communication = new nexilis::client::Room::Communication(payload, sender->client);
        communication->communication = cpp_communication;
        return communication;
    }
    return nullptr;
}

void nexilis_communication_destroy(nexilis_Communication* communication)
{
    if (communication)
    {
        if (communication->communication)
        {
            delete communication->communication;
            communication->communication = nullptr;
        }
        delete communication;
    }
}

const char* nexilis_communication_get_payload(const nexilis_Communication* communication)
{
    if (communication && communication->communication)
    {
        std::string payload = communication->communication->getPayload();
        char* cstr = (char*)malloc(payload.size() + 1);
        if (cstr)
        {
            std::strcpy(cstr, payload.c_str());
        }
        return cstr;
    }
    return nullptr;
}

const nexilis_ClientSession* nexilis_communication_get_client(const nexilis_Communication* communication)
{
    if (communication && communication->communication)
    {
        auto client_session = new nexilis_ClientSession();
        auto cpp_client_session = communication->communication->getClient();
        client_session->client = const_cast<nexilis::client::ClientSession*>(cpp_client_session);
        return client_session;
    }
    return nullptr;
}

uint64_t nexilis_communication_get_id(const nexilis_Communication* communication)
{
    if (communication && communication->communication)
    {
        return communication->communication->getId();
    }
    return 0;
}

uint64_t nexilis_communication_get_sender_id(const nexilis_Communication* communication)
{
    if (!communication || !communication->communication)
        return 0;
    return communication->communication->getSenderId();
}
