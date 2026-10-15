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

#include <nexilis/client/packet.hh>
#include <nexilis/logger/log.hh>
#include <nexilis/ports.hh>
#include <nexilis/protocol_manager.hh>
#include <nexilis/server/client_storage.hh>

#include <nexilis/client/protocol/af_inet/udp_client.hh>
#include <nexilis/server/protocol/af_inet/udp_server.hh>

#include <chrono>
#include <thread>

class AFInetUDPTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nexilis::Log::startConsoleDebugging();

        settings.setMode(nexilis::server::AuthenticationMode::password_protected);
        settings.setPassphrase("salasana");
        settings.setRootPassword("root");

        server = std::make_shared<nexilis::server::af_inet::UDPServer>(
                protocol_manager.createProtocol<nexilis::server::af_inet::UDPServer>(settings, port));
        server->start();

        std::this_thread::sleep_for(std::chrono::seconds(1));

        nexilis::client::ClientConfig server_data;
        server_data.setPassword("salasana");
        server_data.setInetUDP("127.0.0.1");
        server_data.setInetUDPServerPort(port);
        server_data.setMode(nexilis::server::AuthenticationMode::password_protected);

        api = std::make_unique<nexilis::client::ClientAPI>(server_data);
        client = std::make_shared<nexilis::client::af_inet::UDPClient>(
                protocol_manager.createProtocol<nexilis::client::af_inet::UDPClient>(*api));
    }

    void TearDown() override
    {
        if (client)
        {
            client->stop();
        }
        if (server)
        {
            server->stop();
        }
        nexilis::server::ClientStorage::clear();
        nexilis::Log::stopLogging();
    }

    void startClientAndConnect()
    {
        client->start();

        const auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!client->isConnected() && std::chrono::steady_clock::now() < timeout)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        ASSERT_TRUE(client->isConnected());
    }

    nexilis::ProtocolManager protocol_manager;
    nexilis::server::ServerConfig settings;

    static constexpr uint16_t port = 55211;

    std::unique_ptr<nexilis::client::ClientAPI> api;
    std::shared_ptr<nexilis::server::af_inet::UDPServer> server;
    std::shared_ptr<nexilis::client::af_inet::UDPClient> client;
};

TEST_F(AFInetUDPTest, UDPClientConnectsToServer)
{
    startClientAndConnect();
    EXPECT_EQ(server->getPort(), port);
}

TEST_F(AFInetUDPTest, SendMessageAsyncReturnsReadyFuture)
{
    startClientAndConnect();

    auto payload = nexilis::client::Packet::Room::Management::create(*api, "async_room");
    auto future = client->sendMessageAsync(payload);

    auto status = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(status, std::future_status::ready);

    EXPECT_NO_THROW(future.get());
}

TEST_F(AFInetUDPTest, SendMessageAsyncReturnsReadyFutureOnStoppedClient)
{
    // Don't start the client.
    auto payload = nexilis::client::Packet::Room::Management::create(*api, "test");
    auto future = client->sendMessageAsync(payload);

    auto status = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(status, std::future_status::ready);

    // Should throw since the client is stopped.
    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST_F(AFInetUDPTest, SendMessageAsyncReturnsReadyFutureOnClosedSocket)
{
    startClientAndConnect();

    client->stop();

    auto payload = nexilis::client::Packet::Room::Management::create(*api, "test");
    auto future = client->sendMessageAsync(payload);

    auto status = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(status, std::future_status::ready);

    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST_F(AFInetUDPTest, SendMessageAsyncDeliversMessage)
{
    startClientAndConnect();

    EXPECT_EQ(api->getActiveRooms().size(), 0);
    auto payload = nexilis::client::Packet::Room::Management::create(*api, "async_room");
    auto future = client->sendMessageAsync(payload);

    auto status = future.wait_for(std::chrono::seconds(5));
    ASSERT_EQ(status, std::future_status::ready);
    EXPECT_NO_THROW(future.get());

    const auto room_timeout = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (api->getActiveRooms().size() == 0 && std::chrono::steady_clock::now() < room_timeout)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    EXPECT_EQ(api->getActiveRooms().size(), 1);
}
