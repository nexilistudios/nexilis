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
#include <nexilisc/client/packet_c.h>
#include <nexilisc/nx_data_c.h>
#include <nexilisc/types/vector3_c.h>

#include <cstring>

class PacketTest_c : public ::testing::Test
{
protected:
    nexilis_ClientConfigC* config = nullptr;
    nexilis_ClientAPI* client_api = nullptr;

    void SetUp() override
    {
        config = nexilis_client_config_create();
        client_api = nexilis_client_api_create(config);
    }

    void TearDown() override
    {
        if (client_api)
            nexilis_client_api_destroy(client_api);
        if (config)
            nexilis_client_config_destroy(config);
    }
};

TEST_F(PacketTest_c, GetInfoGeneral)
{
    nx_data_c packet = nexilis_packet_get_info_general(client_api);
    EXPECT_NE(packet.data, nullptr);
    EXPECT_GT(nexilis_nx_data_get_size(&packet), 0u);
    nexilis_nx_data_destroy(&packet);
}

TEST_F(PacketTest_c, GetInfoClients)
{
    nx_data_c packet = nexilis_packet_get_info_clients(client_api);
    EXPECT_NE(packet.data, nullptr);
    EXPECT_GT(nexilis_nx_data_get_size(&packet), 0u);
    nexilis_nx_data_destroy(&packet);
}

TEST_F(PacketTest_c, GetInfoRooms)
{
    nx_data_c packet = nexilis_packet_get_info_rooms(client_api);
    EXPECT_NE(packet.data, nullptr);
    EXPECT_GT(nexilis_nx_data_get_size(&packet), 0u);
    nexilis_nx_data_destroy(&packet);
}

TEST_F(PacketTest_c, RoomManagementJoin)
{
    nx_data_c packet = nexilis_packet_room_management_join(client_api, 42);
    EXPECT_NE(packet.data, nullptr);
    EXPECT_GT(nexilis_nx_data_get_size(&packet), 0u);
    nexilis_nx_data_destroy(&packet);
}

TEST_F(PacketTest_c, RoomManagementLeave)
{
    nx_data_c packet = nexilis_packet_room_management_leave(client_api);
    EXPECT_NE(packet.data, nullptr);
    EXPECT_GT(nexilis_nx_data_get_size(&packet), 0u);
    nexilis_nx_data_destroy(&packet);
}

TEST_F(PacketTest_c, RoomManagementCreate)
{
    nx_data_c packet = nexilis_packet_room_management_create(client_api, "NewRoom", ROOM_CONTEXT_3D);
    EXPECT_NE(packet.data, nullptr);
    EXPECT_GT(nexilis_nx_data_get_size(&packet), 0u);
    nexilis_nx_data_destroy(&packet);
}

TEST_F(PacketTest_c, RoomManagementCreate2D)
{
    nx_data_c packet = nexilis_packet_room_management_create(client_api, "FlatRoom", ROOM_CONTEXT_2D);
    EXPECT_NE(packet.data, nullptr);
    EXPECT_GT(nexilis_nx_data_get_size(&packet), 0u);
    nexilis_nx_data_destroy(&packet);
}

TEST_F(PacketTest_c, Player3DPositionDirect)
{
    nx_data_c packet = nexilis_packet_room_player3D_position_direct(client_api, 1.0f, 2.0f, 3.0f);
    EXPECT_NE(packet.data, nullptr);
    EXPECT_GT(nexilis_nx_data_get_size(&packet), 0u);
    nexilis_nx_data_destroy(&packet);
}

TEST_F(PacketTest_c, Player3DPosition)
{
    nexilis_Vector3f* pos = nexilis_vector3f_create(10.0f, 20.0f, 30.0f);
    nx_data_c packet = nexilis_packet_room_player3D_position(client_api, pos);
    EXPECT_NE(packet.data, nullptr);
    EXPECT_GT(nexilis_nx_data_get_size(&packet), 0u);
    nexilis_nx_data_destroy(&packet);
    nexilis_vector3f_destroy(pos);
}

TEST_F(PacketTest_c, Player3DDimension)
{
    nexilis_Vector3f* dim = nexilis_vector3f_create(1.0f, 1.0f, 1.0f);
    nx_data_c packet = nexilis_packet_room_player3D_dimension(client_api, dim);
    EXPECT_NE(packet.data, nullptr);
    EXPECT_GT(nexilis_nx_data_get_size(&packet), 0u);
    nexilis_nx_data_destroy(&packet);
    nexilis_vector3f_destroy(dim);
}

TEST_F(PacketTest_c, Player3DMovementDirect)
{
    nx_data_c packet = nexilis_packet_room_player3D_movement_direct(client_api, 0.1f, 0.2f, 0.3f, 0.016f);
    EXPECT_NE(packet.data, nullptr);
    EXPECT_GT(nexilis_nx_data_get_size(&packet), 0u);
    nexilis_nx_data_destroy(&packet);
}

TEST_F(PacketTest_c, Player3DMovement)
{
    nexilis_Vector3f* mov = nexilis_vector3f_create(0.5f, 0.0f, -0.5f);
    nx_data_c packet = nexilis_packet_room_player3D_movement(client_api, mov, 0.033f);
    EXPECT_NE(packet.data, nullptr);
    EXPECT_GT(nexilis_nx_data_get_size(&packet), 0u);
    nexilis_nx_data_destroy(&packet);
    nexilis_vector3f_destroy(mov);
}

TEST_F(PacketTest_c, Player3DFire)
{
    nx_data_c packet = nexilis_packet_room_player3D_fire(client_api, 0.0f, 0.0f, -1.0f, 1);
    EXPECT_NE(packet.data, nullptr);
    EXPECT_EQ(nexilis_nx_data_get_size(&packet), 17u);
    nexilis_nx_data_destroy(&packet);
}
