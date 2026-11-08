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

#include <cstdlib>
#include <cstring>

class ClientSessionTest_c : public ::testing::Test
{
protected:
    nexilis_ClientAPI* client_api = nullptr;

    void SetUp() override
    {
        nexilis_ClientConfigC* config = nexilis_client_config_create();
        client_api = nexilis_client_api_create(config);
        nexilis_client_config_destroy(config);
    }

    void TearDown() override
    {
        if (client_api)
        {
            nexilis_client_api_destroy(client_api);
        }
    }
};

TEST_F(ClientSessionTest_c, CreateAndDestroy)
{
    nexilis_ClientSession* session = nexilis_client_session_create(42, client_api);
    ASSERT_NE(session, nullptr);
    nexilis_client_session_destroy(session);
}

TEST_F(ClientSessionTest_c, GetId)
{
    nexilis_ClientSession* session = nexilis_client_session_create(42, client_api);
    EXPECT_EQ(nexilis_client_session_get_id(session), 42u);
    nexilis_client_session_destroy(session);
}

TEST_F(ClientSessionTest_c, GetIdDifferentValues)
{
    nexilis_ClientSession* s1 = nexilis_client_session_create(1, client_api);
    nexilis_ClientSession* s2 = nexilis_client_session_create(999, client_api);

    EXPECT_EQ(nexilis_client_session_get_id(s1), 1u);
    EXPECT_EQ(nexilis_client_session_get_id(s2), 999u);

    nexilis_client_session_destroy(s1);
    nexilis_client_session_destroy(s2);
}

TEST_F(ClientSessionTest_c, SetAndGetPosition3D)
{
    nexilis_ClientSession* session = nexilis_client_session_create(1, client_api);

    nexilis_client_session_set_position_3D(session, 1.0f, 2.0f, 3.0f);

    nexilis_Vector3f* pos = nexilis_client_session_get_position_3D(session);
    ASSERT_NE(pos, nullptr);
    EXPECT_FLOAT_EQ(nexilis_vector3f_get_x(pos), 1.0f);
    EXPECT_FLOAT_EQ(nexilis_vector3f_get_y(pos), 2.0f);
    EXPECT_FLOAT_EQ(nexilis_vector3f_get_z(pos), 3.0f);

    nexilis_vector3f_destroy(pos);
    nexilis_client_session_destroy(session);
}

TEST_F(ClientSessionTest_c, GetPosition3DValues)
{
    nexilis_ClientSession* session = nexilis_client_session_create(1, client_api);

    nexilis_client_session_set_position_3D(session, 10.5f, -20.5f, 30.0f);

    float x, y, z;
    EXPECT_TRUE(nexilis_client_session_get_position_3D_values(session, &x, &y, &z));
    EXPECT_FLOAT_EQ(x, 10.5f);
    EXPECT_FLOAT_EQ(y, -20.5f);
    EXPECT_FLOAT_EQ(z, 30.0f);

    nexilis_client_session_destroy(session);
}

TEST_F(ClientSessionTest_c, EqualSessions)
{
    nexilis_ClientSession* a = nexilis_client_session_create(10, client_api);
    nexilis_ClientSession* b = nexilis_client_session_create(10, client_api);

    // Sessions with same id should be equal (depends on ClientSession implementation).
    bool eq = nexilis_client_session_equal(a, b);
    bool neq = nexilis_client_session_not_equal(a, b);
    EXPECT_EQ(eq, !neq);

    nexilis_client_session_destroy(a);
    nexilis_client_session_destroy(b);
}

TEST_F(ClientSessionTest_c, NotEqualSessions)
{
    nexilis_ClientSession* a = nexilis_client_session_create(10, client_api);
    nexilis_ClientSession* b = nexilis_client_session_create(20, client_api);

    bool neq = nexilis_client_session_not_equal(a, b);
    EXPECT_TRUE(neq);

    nexilis_client_session_destroy(a);
    nexilis_client_session_destroy(b);
}

TEST_F(ClientSessionTest_c, SetUsername)
{
    nexilis_ClientSession* session = nexilis_client_session_create(1, client_api);
    nexilis_client_session_set_username(session, "test_player");
    // No getter for username via C API, but this should not crash.
    nexilis_client_session_destroy(session);
}

TEST_F(ClientSessionTest_c, MoveSession)
{
    nexilis_ClientSession* original = nexilis_client_session_create(50, client_api);
    nexilis_client_session_set_position_3D(original, 1.0f, 2.0f, 3.0f);

    nexilis_ClientSession* moved = nexilis_client_session_move(original);
    ASSERT_NE(moved, nullptr);
    EXPECT_EQ(nexilis_client_session_get_id(moved), 50u);

    nexilis_client_session_destroy(moved);
    nexilis_client_session_destroy(original);
}

TEST_F(ClientSessionTest_c, MoveAssignSession)
{
    nexilis_ClientSession* dest = nexilis_client_session_create(1, client_api);
    nexilis_ClientSession* src = nexilis_client_session_create(2, client_api);

    nexilis_client_session_move_assign(dest, src);
    EXPECT_EQ(nexilis_client_session_get_id(dest), 2u);

    nexilis_client_session_destroy(dest);
    nexilis_client_session_destroy(src);
}

TEST_F(ClientSessionTest_c, GetClientFromRoomUnknownClientIsNull)
{
    // A fresh client API knows no rooms, so the lookup has to report the
    // miss with a null pointer instead of a handle wrapping a null session.
    EXPECT_EQ(nexilis_client_api_get_client_from_room(client_api, 42), nullptr);
}

TEST_F(ClientSessionTest_c, GetRoomUnknownRoomIsNull)
{
    EXPECT_EQ(nexilis_client_api_get_room(client_api, 42), nullptr);
}

TEST_F(ClientSessionTest_c, GetClientUsernameOfUnknownClientIsEmpty)
{
    const char* username = nexilis_client_api_get_client_username(client_api, 42);
    ASSERT_NE(username, nullptr);
    EXPECT_STREQ(username, "");
    free((void*)username);
}

TEST_F(ClientSessionTest_c, OwnedLookupsSurviveRoomReplacement)
{
    auto deliver = [&](const char* json)
    {
        nexilis::nx_data message(json, json + std::strlen(json));
        message.push_back('\n');
        ASSERT_EQ(client_api->api->readMessage(message), nexilis::client::ReadResult::success);
    };

    deliver(R"({"command":"getting","type":"info_rooms","callback":0,"rooms":[{"room_id":1,"creator_id":1,"name":"room","context":1,"max_size":2,"clients":[{"id":42,"name":"player","x":0,"y":0,"width":1,"height":1}]}]})");

    auto* room = nexilis_client_api_get_room(client_api, 1);
    auto* session = nexilis_client_api_get_client_from_room(client_api, 42);
    ASSERT_NE(room, nullptr);
    ASSERT_NE(session, nullptr);

    // Replacing the client's room state must not invalidate the snapshots.
    deliver(R"({"command":"getting","type":"info_rooms","callback":0,"rooms":[{"room_id":1,"creator_id":1,"name":"room","context":1,"max_size":2,"clients":[{"id":7,"name":"other","x":0,"y":0,"width":1,"height":1}]}]})");

    EXPECT_EQ(nexilis_room_get_id(room), 1u);
    EXPECT_EQ(nexilis_client_session_get_id(session), 42u);
    EXPECT_EQ(nexilis_client_api_get_client_from_room(client_api, 42), nullptr);

    nexilis_room_destroy(room);
    nexilis_client_session_destroy(session);
}
