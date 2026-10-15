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

#include <nexilisc/protocol_manager_c.h>
#include <nexilisc/protocol_type_c.h>

class ProtocolManagerExtendedTest_c : public ::testing::Test
{
protected:
    nexilis_ProtocolManagerC* manager = nullptr;

    void SetUp() override
    {
        manager = nexilis_protocol_manager_create();
    }

    void TearDown() override
    {
        nexilis_protocol_manager_destroy(manager);
    }
};

TEST_F(ProtocolManagerExtendedTest_c, ProtocolDataCreateDestroy)
{
    nexilis_ProtocolDataC* data = nexilis_protocol_data_create(PROTOCOL_TYPE_BOOST_TCP_SERVER);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(nexilis_protocol_data_get_type(data), PROTOCOL_TYPE_BOOST_TCP_SERVER);
    EXPECT_NE(nexilis_protocol_data_get_id(data), 0u);
    nexilis_protocol_data_destroy(data);
}

TEST_F(ProtocolManagerExtendedTest_c, ProtocolDataIdUniqueness)
{
    nexilis_ProtocolDataC* a = nexilis_protocol_data_create(PROTOCOL_TYPE_BOOST_TCP_SERVER);
    nexilis_ProtocolDataC* b = nexilis_protocol_data_create(PROTOCOL_TYPE_BOOST_TCP_SERVER);

    EXPECT_NE(nexilis_protocol_data_get_id(a), nexilis_protocol_data_get_id(b));

    nexilis_protocol_data_destroy(a);
    nexilis_protocol_data_destroy(b);
}

TEST(ProtocolPortsTest_c, BoostPortsCanBeOverridden)
{
    const auto tcp = nexilis_get_boost_tcp_port();
    const auto udp = nexilis_get_boost_udp_port();
    const auto session = nexilis_get_boost_tcp_session_port();

    nexilis_set_boost_tcp_port(55700);
    nexilis_set_boost_udp_port(55701);
    nexilis_set_boost_tcp_session_port(55702);

    EXPECT_EQ(nexilis_get_boost_tcp_port(), 55700);
    EXPECT_EQ(nexilis_get_boost_udp_port(), 55701);
    EXPECT_EQ(nexilis_get_boost_tcp_session_port(), 55702);

    nexilis_set_boost_tcp_port(tcp);
    nexilis_set_boost_udp_port(udp);
    nexilis_set_boost_tcp_session_port(session);
}
