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

#include <nexilisc/room_data_c.h>

#include <cstdlib>
#include <cstring>
#include <string>

TEST(RoomDataTest_c, CreateWithAllParams)
{
    nexilis_RoomData* room = nexilis_room_data_create(100, "TestRoom", ROOM_CONTEXT_2D, 10);
    ASSERT_NE(room, nullptr);

    EXPECT_EQ(nexilis_room_data_get_creator_id(room), 100u);

    const char* name = nexilis_room_data_get_name(room);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "TestRoom");
    free((void*)name);

    EXPECT_EQ(nexilis_room_data_get_context(room), ROOM_CONTEXT_2D);
    EXPECT_EQ(nexilis_room_data_get_max_size(room), 10u);
    EXPECT_NE(nexilis_room_data_get_id(room), 0u);

    nexilis_room_data_destroy(room);
}

TEST(RoomDataTest_c, NullNameFailsWithoutThrowing)
{
    EXPECT_EQ(nexilis_room_data_create(1, nullptr, ROOM_CONTEXT_3D, 20), nullptr);
}

TEST(RoomDataTest_c, CreateWith3DContext)
{
    nexilis_RoomData* room = nexilis_room_data_create(1, "MyRoom", ROOM_CONTEXT_3D, 20);
    ASSERT_NE(room, nullptr);

    EXPECT_EQ(nexilis_room_data_get_creator_id(room), 1u);

    const char* name = nexilis_room_data_get_name(room);
    EXPECT_STREQ(name, "MyRoom");
    free((void*)name);

    EXPECT_EQ(nexilis_room_data_get_context(room), ROOM_CONTEXT_3D);
    EXPECT_EQ(nexilis_room_data_get_max_size(room), 20u);

    nexilis_room_data_destroy(room);
}

TEST(RoomDataTest_c, DestroyNullSafe)
{
    nexilis_room_data_destroy(nullptr);
}

TEST(RoomDataTest_c, GetNameNullSafe)
{
    EXPECT_EQ(nexilis_room_data_get_name(nullptr), nullptr);
}

TEST(RoomDataTest_c, GetContextNullSafe)
{
    EXPECT_EQ(nexilis_room_data_get_context(nullptr), ROOM_CONTEXT_UNDEFINED);
}

TEST(RoomDataTest_c, GetMaxSizeNullSafe)
{
    EXPECT_EQ(nexilis_room_data_get_max_size(nullptr), 0u);
}

TEST(RoomDataTest_c, GetIdNullSafe)
{
    EXPECT_EQ(nexilis_room_data_get_id(nullptr), 0u);
}

TEST(RoomDataTest_c, GetCreatorIdNullSafe)
{
    EXPECT_EQ(nexilis_room_data_get_creator_id(nullptr), 0u);
}

TEST(RoomDataTest_c, UniqueIds)
{
    nexilis_RoomData* a = nexilis_room_data_create(0, "RoomA", ROOM_CONTEXT_2D, 10);
    nexilis_RoomData* b = nexilis_room_data_create(0, "RoomB", ROOM_CONTEXT_3D, 5);

    EXPECT_NE(nexilis_room_data_get_id(a), nexilis_room_data_get_id(b));

    nexilis_room_data_destroy(a);
    nexilis_room_data_destroy(b);
}

TEST(RoomDataTest_c, ContextEnumValues)
{
    nexilis_RoomData* room_2d = nexilis_room_data_create(0, "R2D", ROOM_CONTEXT_2D, 10);
    nexilis_RoomData* room_3d = nexilis_room_data_create(0, "R3D", ROOM_CONTEXT_3D, 10);

    EXPECT_EQ(nexilis_room_data_get_context(room_2d), ROOM_CONTEXT_2D);
    EXPECT_EQ(nexilis_room_data_get_context(room_3d), ROOM_CONTEXT_3D);

    nexilis_room_data_destroy(room_2d);
    nexilis_room_data_destroy(room_3d);
}
