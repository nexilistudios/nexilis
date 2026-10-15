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

#include <nexilis/nx_protocol.hh>
#include <nexilis/protocol_manager.hh>
#include <nexilis/server/server_config.hh>
#include <nexilis/tcp_client.hh>
#include <nexilis/udp_client.hh>

#include <nexilis/client/client_api.hh>
#include <nexilis/client/client_config.hh>
#include <nexilis/client/protocol/af_inet/udp_client.hh>
#include <nexilis/server/protocol/af_inet/udp_server.hh>
#include <nexilis/server/protocol/af_unix/dgram_server.hh>
#include <nexilis/server/protocol/af_unix/stream_server.hh>
#include <nexilis/server/protocol/nxboost/tcp_server.hh>

class ProtocolManagerTest : public ::testing::Test
{
protected:
    nexilis::ProtocolManager manager;
};

TEST_F(ProtocolManagerTest, CreateProtocol_UnixStreamServer)
{
    nexilis::server::ServerConfig settings;
    settings.setMode(nexilis::server::AuthenticationMode::password_protected);
    settings.setPassphrase("salasana");
    settings.setRootPassword("root");

    auto unix_stream_server = manager.createProtocol<nexilis::server::af_unix::StreamServer>(settings, "/tmp/nexilis/stream");

    EXPECT_EQ(unix_stream_server.getType(), nexilis::NXProtocol::Type::AF_UNIX_SOCK_STREAM_SERVER);
    EXPECT_EQ(unix_stream_server.getSettings().getMode(), nexilis::server::AuthenticationMode::password_protected);
    EXPECT_EQ(unix_stream_server.getSettings().getPassphrase(), "salasana");
    EXPECT_EQ(unix_stream_server.getSettings().getRootPassword(), "root");
}

TEST_F(ProtocolManagerTest, CreateProtocol_UnixDgramServer)
{
    nexilis::server::ServerConfig settings;
    settings.setMode(nexilis::server::AuthenticationMode::password_protected);
    settings.setPassphrase("salasana");
    settings.setRootPassword("root");

    auto unix_dgram_server = manager.createProtocol<nexilis::server::af_unix::DgramServer>(settings, "/tmp/nexilis/dgram");

    EXPECT_EQ(unix_dgram_server.getType(), nexilis::NXProtocol::Type::AF_UNIX_SOCK_DGRAM_SERVER);
    EXPECT_EQ(unix_dgram_server.getSettings().getMode(), nexilis::server::AuthenticationMode::password_protected);
    EXPECT_EQ(unix_dgram_server.getSettings().getPassphrase(), "salasana");
    EXPECT_EQ(unix_dgram_server.getSettings().getRootPassword(), "root");
}

TEST_F(ProtocolManagerTest, CreateProtocol_AFInetUDPServer)
{
    nexilis::server::ServerConfig settings;
    settings.setMode(nexilis::server::AuthenticationMode::password_protected);
    settings.setPassphrase("salasana");
    settings.setRootPassword("root");

    auto inet_udp_server = manager.createProtocol<nexilis::server::af_inet::UDPServer>(settings, 55211);

    EXPECT_EQ(inet_udp_server.getType(), nexilis::NXProtocol::Type::AF_INET_UDP_SERVER);
    EXPECT_EQ(inet_udp_server.getPort(), 55211);
    EXPECT_EQ(inet_udp_server.getSettings().getMode(), nexilis::server::AuthenticationMode::password_protected);
    EXPECT_EQ(inet_udp_server.getSettings().getPassphrase(), "salasana");
    EXPECT_EQ(inet_udp_server.getSettings().getRootPassword(), "root");
}

TEST_F(ProtocolManagerTest, CreateProtocol_BoostTCPServer)
{
    nexilis::server::ServerConfig settings;
    settings.setMode(nexilis::server::AuthenticationMode::password_protected);
    settings.setPassphrase("salasana");
    settings.setRootPassword("root");

    auto boost_tcp_server = manager.createProtocol<nexilis::server::nxboost::TCPServer>(settings);

    EXPECT_EQ(boost_tcp_server.getType(), nexilis::NXProtocol::Type::BOOST_TCP_SERVER);
    EXPECT_EQ(boost_tcp_server.getSettings().getMode(), nexilis::server::AuthenticationMode::password_protected);
    EXPECT_EQ(boost_tcp_server.getSettings().getPassphrase(), "salasana");
    EXPECT_EQ(boost_tcp_server.getSettings().getRootPassword(), "root");
}

TEST_F(ProtocolManagerTest, CreateProtocol_RootTCPClient)
{
    nexilis::client::ClientConfig config;
    config.setPassword("salasana");
    config.setBoostTCPAddress("127.0.0.1");
    config.setMode(nexilis::server::AuthenticationMode::password_protected);

    nexilis::client::ClientAPI client_api(config);
    auto tcp_client = manager.createProtocol<nexilis::TCPClient>(client_api);

    EXPECT_EQ(tcp_client.getType(), nexilis::NXProtocol::Type::BOOST_TCP_CLIENT);
}

TEST_F(ProtocolManagerTest, CreateProtocol_RootUDPClient)
{
    nexilis::client::ClientConfig config;
    config.setPassword("salasana");
    config.setBoostUDPAddress("127.0.0.1");
    config.setMode(nexilis::server::AuthenticationMode::password_protected);

    nexilis::client::ClientAPI client_api(config);
    auto udp_client = manager.createProtocol<nexilis::UDPClient>(client_api);

    EXPECT_EQ(udp_client.getType(), nexilis::NXProtocol::Type::BOOST_UDP_CLIENT);
}

TEST_F(ProtocolManagerTest, CreateProtocol_AFInetUDPClient)
{
    nexilis::client::ClientConfig config;
    config.setPassword("salasana");
    config.setInetUDP("127.0.0.1");
    config.setInetUDPServerPort(55211);
    config.setMode(nexilis::server::AuthenticationMode::password_protected);

    nexilis::client::ClientAPI client_api(config);
    auto udp_client = manager.createProtocol<nexilis::client::af_inet::UDPClient>(client_api);

    EXPECT_EQ(udp_client.getType(), nexilis::NXProtocol::Type::AF_INET_UDP_CLIENT);
}

TEST_F(ProtocolManagerTest, GetProtocolCount_Empty)
{
    EXPECT_EQ(manager.getProtocolCount(), 0u);
}

TEST_F(ProtocolManagerTest, GetProtocolCount_AfterCreate)
{
    nexilis::server::ServerConfig settings;
    settings.setMode(nexilis::server::AuthenticationMode::password_protected);
    settings.setPassphrase("salasana");
    settings.setRootPassword("root");

    manager.createProtocol<nexilis::server::nxboost::TCPServer>(settings);
    EXPECT_EQ(manager.getProtocolCount(), 1u);

    manager.createProtocol<nexilis::server::af_unix::StreamServer>(settings, "/tmp/nexilis/stream");
    EXPECT_EQ(manager.getProtocolCount(), 2u);
}

TEST_F(ProtocolManagerTest, HasProtocol_Found)
{
    nexilis::server::ServerConfig settings;
    settings.setMode(nexilis::server::AuthenticationMode::password_protected);
    settings.setPassphrase("salasana");
    settings.setRootPassword("root");

    manager.createProtocol<nexilis::server::nxboost::TCPServer>(settings);
    ASSERT_EQ(manager.getProtocolCount(), 1u);
    const auto* data = manager.findById(manager.begin()->getId());
    ASSERT_NE(data, nullptr);
    EXPECT_TRUE(manager.hasProtocol(data->getId()));
}

TEST_F(ProtocolManagerTest, HasProtocol_NotFound)
{
    EXPECT_FALSE(manager.hasProtocol(12345u));
}

TEST_F(ProtocolManagerTest, HasProtocolType_Found)
{
    nexilis::server::ServerConfig settings;
    settings.setMode(nexilis::server::AuthenticationMode::password_protected);
    settings.setPassphrase("salasana");
    settings.setRootPassword("root");

    manager.createProtocol<nexilis::server::nxboost::TCPServer>(settings);
    EXPECT_TRUE(manager.hasProtocolType(nexilis::NXProtocol::Type::BOOST_TCP_SERVER));
}

TEST_F(ProtocolManagerTest, HasProtocolType_NotFound)
{
    EXPECT_FALSE(manager.hasProtocolType(nexilis::NXProtocol::Type::BOOST_UDP_SERVER));
}

TEST_F(ProtocolManagerTest, FindById_Found)
{
    nexilis::server::ServerConfig settings;
    settings.setMode(nexilis::server::AuthenticationMode::password_protected);
    settings.setPassphrase("salasana");
    settings.setRootPassword("root");

    auto server = manager.createProtocol<nexilis::server::nxboost::TCPServer>(settings);
    // The ProtocolData is stored with the id from getType() call during creation.
    // We need to iterate to find the matching one since we don't have the id directly.
    bool found = false;
    for (const auto& item : manager)
    {
        if (item.getType() == nexilis::NXProtocol::Type::BOOST_TCP_SERVER)
        {
            const auto* data = manager.findById(item.getId());
            ASSERT_NE(data, nullptr);
            EXPECT_EQ(data->getType(), nexilis::NXProtocol::Type::BOOST_TCP_SERVER);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(ProtocolManagerTest, FindById_NotFound)
{
    EXPECT_EQ(manager.findById(99999u), nullptr);
}

TEST_F(ProtocolManagerTest, FindByType_Found)
{
    nexilis::server::ServerConfig settings;
    settings.setMode(nexilis::server::AuthenticationMode::password_protected);
    settings.setPassphrase("salasana");
    settings.setRootPassword("root");

    manager.createProtocol<nexilis::server::nxboost::TCPServer>(settings);
    auto results = manager.findByType(nexilis::NXProtocol::Type::BOOST_TCP_SERVER);

    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0]->getType(), nexilis::NXProtocol::Type::BOOST_TCP_SERVER);
}

TEST_F(ProtocolManagerTest, FindByType_NotFound)
{
    auto results = manager.findByType(nexilis::NXProtocol::Type::BOOST_UDP_SERVER);
    EXPECT_TRUE(results.empty());
}

TEST_F(ProtocolManagerTest, FindByType_Multiple)
{
    nexilis::server::ServerConfig settings;
    settings.setMode(nexilis::server::AuthenticationMode::password_protected);
    settings.setPassphrase("salasana");
    settings.setRootPassword("root");

    manager.createProtocol<nexilis::server::nxboost::TCPServer>(settings);
    manager.createProtocol<nexilis::server::af_unix::StreamServer>(settings, "/tmp/nexilis/stream");
    manager.createProtocol<nexilis::server::nxboost::TCPServer>(settings);

    auto tcp_results = manager.findByType(nexilis::NXProtocol::Type::BOOST_TCP_SERVER);
    EXPECT_EQ(tcp_results.size(), 2u);

    auto unix_results = manager.findByType(nexilis::NXProtocol::Type::AF_UNIX_SOCK_STREAM_SERVER);
    EXPECT_EQ(unix_results.size(), 1u);
}

TEST_F(ProtocolManagerTest, Iterator_Empty)
{
    EXPECT_EQ(manager.begin(), manager.end());
}

TEST_F(ProtocolManagerTest, Iterator_NonEmpty)
{
    nexilis::server::ServerConfig settings;
    settings.setMode(nexilis::server::AuthenticationMode::password_protected);
    settings.setPassphrase("salasana");
    settings.setRootPassword("root");

    manager.createProtocol<nexilis::server::nxboost::TCPServer>(settings);
    manager.createProtocol<nexilis::server::af_unix::StreamServer>(settings, "/tmp/nexilis/stream");

    size_t count = 0;
    for (const auto& item : manager)
    {
        EXPECT_NE(item.getType(), nexilis::NXProtocol::Type::UNKNOWN);
        ++count;
    }
    EXPECT_EQ(count, 2u);
}

TEST_F(ProtocolManagerTest, ProtocolData_IdUniqueness)
{
    nexilis::server::ServerConfig settings;
    settings.setMode(nexilis::server::AuthenticationMode::password_protected);
    settings.setPassphrase("salasana");
    settings.setRootPassword("root");

    manager.createProtocol<nexilis::server::nxboost::TCPServer>(settings);
    manager.createProtocol<nexilis::server::af_unix::StreamServer>(settings, "/tmp/nexilis/stream");

    // Two creates should produce entries with different ids.
    ASSERT_EQ(manager.getProtocolCount(), 2u);
    auto it = manager.begin();
    auto id1 = it->getId();
    ++it;
    auto id2 = it->getId();
    EXPECT_NE(id1, id2);
}
