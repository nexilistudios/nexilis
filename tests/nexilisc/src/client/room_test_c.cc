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

#include <gtest/gtest.h>

#include <nexilisc/client/client_api_c.h>
#include <nexilisc/client/client_config_c.h>
#include <nexilisc/client/client_session_c.h>
#include <nexilisc/client/room_c.h>
#include <nexilisc/client/rooms_collection.h>
#include <nexilisc/room_data_c.h>

#include <cstdlib>
#include <cstring>
#include <nexilisc/nexilis_promise.h>
#include <nexilisc/nexilis_waiter.h>

class RoomTest_c : public ::testing::Test
{
protected:
    nexilis_ClientConfigC* client_config = nullptr;
    nexilis_ClientAPI* client_api = nullptr;
    nexilis_RoomData* room_data = nullptr;
    nexilis_Room* room = nullptr;

    void SetUp() override
    {
        client_config = nexilis_client_config_create();
        client_api = nexilis_client_api_create(client_config);
        room_data = nexilis_room_data_create(100, "TestRoom", ROOM_CONTEXT_3D, 10);
        room = nexilis_room_create(room_data, nullptr);
    }

    void TearDown() override
    {
        if (room)
            nexilis_room_destroy(room);
        if (room_data)
            nexilis_room_data_destroy(room_data);
        if (client_api)
            nexilis_client_api_destroy(client_api);
        if (client_config)
            nexilis_client_config_destroy(client_config);
    }

    nexilis_ClientSession* makeSession(uint64_t id)
    {
        return nexilis_client_session_create(id, client_api);
    }
};

TEST_F(RoomTest_c, CreateAndDestroy)
{
    ASSERT_NE(room, nullptr);
    EXPECT_EQ(nexilis_room_get_client_amount(room), 0u);
}

TEST_F(RoomTest_c, SessionSnapshotSurvivesArrayAndRoomDestruction)
{
    auto source = makeSession(42);
    nexilis_client_session_set_position_3D(source, 1, 2, 3);
    nexilis_room_add_client(room, source);
    nexilis_client_session_destroy(source);

    size_t count = 0;
    auto clients = nexilis_room_get_clients(room, &count);
    ASSERT_EQ(count, 1u);
    auto copy = nexilis_client_session_clone(clients[0]);
    ASSERT_NE(copy, nullptr);
    nexilis_room_free_client_array(clients, count);
    nexilis_room_destroy(room);
    room = nullptr;

    EXPECT_EQ(nexilis_client_session_get_id(copy), 42u);
    float x, y, z;
    ASSERT_TRUE(nexilis_client_session_get_position_3D_values(copy, &x, &y, &z));
    EXPECT_FLOAT_EQ(x, 1);
    EXPECT_FLOAT_EQ(y, 2);
    EXPECT_FLOAT_EQ(z, 3);
    nexilis_client_session_set_username(copy, "snapshot");
    nexilis_client_session_destroy(copy);
    EXPECT_EQ(nexilis_client_session_clone(nullptr), nullptr);
}

TEST_F(RoomTest_c, GetId)
{
    uint64_t expected_id = nexilis_room_data_get_id(room_data);
    EXPECT_EQ(nexilis_room_get_id(room), expected_id);
}

TEST_F(RoomTest_c, GetName)
{
    const char* name = nexilis_room_get_name(room);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "TestRoom");
    free((void*)name);
}

TEST_F(RoomTest_c, GetMaxSize)
{
    EXPECT_EQ(nexilis_room_get_max_size(room), 10u);
}

TEST_F(RoomTest_c, GetContext)
{
    EXPECT_EQ(nexilis_room_get_context(room), ROOM_CONTEXT_3D);
}

TEST_F(RoomTest_c, GetCreatorId)
{
    EXPECT_EQ(nexilis_room_get_creator_id(room), 100u);
}

TEST_F(RoomTest_c, MetadataNullSafe)
{
    EXPECT_EQ(nexilis_room_get_name(nullptr), nullptr);
    EXPECT_EQ(nexilis_room_get_context(nullptr), ROOM_CONTEXT_UNDEFINED);
    EXPECT_EQ(nexilis_room_get_max_size(nullptr), 0u);
    EXPECT_EQ(nexilis_room_get_creator_id(nullptr), 0u);
}

TEST_F(RoomTest_c, AddClient)
{
    nexilis_ClientSession* client = makeSession(10);
    nexilis_room_add_client(room, client);
    EXPECT_EQ(nexilis_room_get_client_amount(room), 1u);
}

TEST_F(RoomTest_c, AddMultipleClients)
{
    nexilis_ClientSession* c1 = makeSession(10);
    nexilis_ClientSession* c2 = makeSession(20);
    nexilis_room_add_client(room, c1);
    nexilis_room_add_client(room, c2);
    EXPECT_EQ(nexilis_room_get_client_amount(room), 2u);
}

TEST_F(RoomTest_c, RemoveClient)
{
    nexilis_ClientSession* c1 = makeSession(10);
    nexilis_ClientSession* c2 = makeSession(20);
    nexilis_room_add_client(room, c1);
    nexilis_room_add_client(room, c2);

    nexilis_room_remove_client(room, 10);
    EXPECT_EQ(nexilis_room_get_client_amount(room), 1u);
}

TEST_F(RoomTest_c, GetClients)
{
    nexilis_ClientSession* c1 = makeSession(10);
    nexilis_room_add_client(room, c1);

    size_t num_clients = 0;
    nexilis_ClientSession** clients = nexilis_room_get_clients(room, &num_clients);
    ASSERT_NE(clients, nullptr);
    EXPECT_EQ(num_clients, 1u);

    nexilis_room_free_client_array(clients, num_clients);
}

TEST_F(RoomTest_c, GetClientsEmpty)
{
    size_t num_clients = 99;
    nexilis_ClientSession** clients = nexilis_room_get_clients(room, &num_clients);
    EXPECT_EQ(clients, nullptr);
    EXPECT_EQ(num_clients, 0u);
}

TEST_F(RoomTest_c, CommunicationCreateAndPayload)
{
    nexilis_ClientSession* sender = makeSession(10);
    nexilis_Communication* comm = nexilis_communication_create("hello world", sender);
    ASSERT_NE(comm, nullptr);

    const char* payload = nexilis_communication_get_payload(comm);
    ASSERT_NE(payload, nullptr);
    EXPECT_STREQ(payload, "hello world");
    free((void*)payload);

    nexilis_communication_destroy(comm);
}

TEST_F(RoomTest_c, CommunicationAddMessage)
{
    nexilis_ClientSession* sender = makeSession(10);
    nexilis_Communication* comm = nexilis_communication_create("test message", sender);
    ASSERT_NE(comm, nullptr);

    nexilis_room_add_message(room, comm);
    EXPECT_NE(nexilis_communication_get_id(comm), 0u);
}

TEST_F(RoomTest_c, CommunicationCreateDestroy)
{
    nexilis_ClientSession* sender = makeSession(10);
    nexilis_Communication* comm = nexilis_communication_create("test", sender);
    ASSERT_NE(comm, nullptr);

    const char* payload = nexilis_communication_get_payload(comm);
    EXPECT_STREQ(payload, "test");
    free((void*)payload);

    nexilis_communication_destroy(comm);
}

TEST_F(RoomTest_c, CommunicationGetClient)
{
    nexilis_ClientSession* sender = makeSession(10);
    nexilis_Communication* comm = nexilis_communication_create("msg", sender);

    const nexilis_ClientSession* retrieved = nexilis_communication_get_client(comm);
    ASSERT_NE(retrieved, nullptr);

    nexilis_communication_destroy(comm);
}

TEST_F(RoomTest_c, CommunicationCreateNullSender)
{
    nexilis_Communication* comm = nexilis_communication_create("test", nullptr);
    EXPECT_EQ(comm, nullptr);
}

TEST_F(RoomTest_c, CommunicationCreateNullPayload)
{
    nexilis_ClientSession* sender = makeSession(10);
    nexilis_Communication* comm = nexilis_communication_create(nullptr, sender);
    EXPECT_EQ(comm, nullptr);
}

TEST_F(RoomTest_c, CommunicationGetMessages)
{
    nexilis_ClientSession* sender = makeSession(10);
    nexilis_Communication* comm = nexilis_communication_create("hello", sender);
    nexilis_room_add_message(room, comm);

    size_t num_messages = 0;
    nexilis_Communication** messages = nexilis_room_get_messages(room, &num_messages);
    ASSERT_NE(messages, nullptr);
    EXPECT_EQ(num_messages, 1u);

    const char* payload = nexilis_communication_get_payload(messages[0]);
    ASSERT_NE(payload, nullptr);
    EXPECT_STREQ(payload, "hello");
    free((void*)payload);

    nexilis_room_free_message_array(messages, num_messages);
}

TEST_F(RoomTest_c, CommunicationGetMessagesEmpty)
{
    size_t num_messages = 99;
    nexilis_Communication** messages = nexilis_room_get_messages(room, &num_messages);
    EXPECT_EQ(messages, nullptr);
    EXPECT_EQ(num_messages, 0u);
}

TEST_F(RoomTest_c, DestroyNullSafe)
{
    nexilis_room_destroy(nullptr);
}

TEST_F(RoomTest_c, CommunicationDestroyNullSafe)
{
    nexilis_communication_destroy(nullptr);
}

TEST_F(RoomTest_c, WaiterRetainsApiAndPromiseAfterHandlesAreDestroyed)
{
    auto promise = nexilis_promise_create();
    auto future = promise->promise->get_future();
    // The callback executes the invalid-type error path without a timed wait.
    auto waiter = nexilis_waiter_create(client_api, promise, static_cast<nexilis_WaitType>(99));
    ASSERT_NE(waiter, nullptr);
    nexilis_promise_destroy(promise);
    nexilis_client_api_destroy(client_api);
    client_api = nullptr;
    nexilis_waiter_wait(waiter);
    ASSERT_EQ(future.wait_for(std::chrono::milliseconds(0)), std::future_status::ready);
    EXPECT_THROW(future.get(), std::runtime_error);
    nexilis_waiter_destroy(waiter);
}

TEST_F(RoomTest_c, WaiterExecutesRoomsCallbackWithRetainedState)
{
    auto promise = nexilis_promise_create();
    auto future = promise->promise->get_future();
    auto waiter = nexilis_waiter_create_for_rooms_created(client_api, promise);
    ASSERT_NE(waiter, nullptr);
    nexilis::nx_data message;
    const std::string json = R"({"command":"getting","type":"info_rooms","callback":0,"rooms":[{"room_id":1,"creator_id":1,"name":"room","context":1,"max_size":2}]})";
    message.assign(json.begin(), json.end());
    message.push_back('\n');
    ASSERT_EQ(client_api->api->readMessage(message), nexilis::client::ReadResult::success);
    nexilis_promise_destroy(promise);
    nexilis_client_api_destroy(client_api);
    client_api = nullptr;
    nexilis_waiter_wait(waiter);
    ASSERT_EQ(future.wait_for(std::chrono::milliseconds(0)), std::future_status::ready);
    EXPECT_NO_THROW(future.get());
    nexilis_waiter_destroy(waiter);
}
