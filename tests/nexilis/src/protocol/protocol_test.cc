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
#include <nexilis/client/protocol/nxboost/tcp_client.hh>
#include <nexilis/client/protocol/nxboost/udp_client.hh>
#include <nexilis/environment.hh>
#include <nexilis/logger/log.hh>
#include <nexilis/protocol_manager.hh>
#include <nexilis/room_data.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/protocol/nxboost/tcp_server.hh>
#include <nexilis/server/protocol/nxboost/udp_server.hh>
#include <nexilis/server/room_storage.hh>

static nexilis::ProtocolManager protocol_manager;

void waitFor(uint32_t seconds, bool condition)
{
    const auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (!condition && std::chrono::steady_clock::now() < timeout)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

template <typename Client, typename Server>
void waitRoomInfo(std::shared_ptr<Client>& client, std::shared_ptr<Server>& server, std::unique_ptr<nexilis::client::ClientAPI>& api)
{
    const auto environment = nexilis::detectRuntimeType();
    const bool is_ci = environment == nexilis::EnvironmentType::ci;
    const auto send_timeout = is_ci ? std::chrono::seconds(10) : std::chrono::seconds(3);

    std::promise<void> promise;
    auto future = promise.get_future();
    client->sendMessage(
            nexilis::client::Packet::Get::Info::rooms(*api),
            api->waitUntilRoomsCreated(promise));

    auto status = future.wait_for(send_timeout);
    if (status != std::future_status::ready)
    {
        std::ostringstream oss;
        oss << "Test timeout - Final state:\n"
            << "  Client connected: " << client->isConnected() << "\n"
            << "  Server connections: " << server->activeConnectionsCount() << "\n"
            << "  Active rooms: " << api->getActiveRooms().size() << "\n"
            << "  Protocol status: " << client->getProtocolStatusString() << std::endl
            << "  CI Environment: " << (is_ci ? "Yes" : "No");
        std::cerr << oss.str() << std::endl;
    }
}

template <typename Server, typename Client>
class ProtocolTest : public ::testing::Test
{
protected:
    virtual void setup() = 0;
    virtual void defaultSetup() = 0;

    void SetUp() override
    {
        nexilis::Log::startConsoleDebugging();
        defaultSetup();
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
        nexilis::Log::stopLogging();
        nexilis::server::ClientStorage::clear();
        nexilis::server::RoomStorage::clear();
    }

    void createSettings()
    {
        settings.setMode(nexilis::server::AuthenticationMode::password_protected);
        settings.setPassphrase("salasana");
        settings.setRootPassword("root");
    }

    void createServer()
    {
        server = std::make_shared<Server>(protocol_manager.createProtocol<Server>(settings));
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    void serverStart()
    {
        server->start();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    void createDefaultServerData()
    {
        server_data.setPassword("salasana");
        server_data.setMode(nexilis::server::AuthenticationMode::password_protected);
    }

    void createClientAPI()
    {
        api = std::make_unique<nexilis::client::ClientAPI>(server_data);
    }

    void createClient()
    {
        client = std::make_shared<Client>(protocol_manager.createProtocol<Client>(*api));
    }

    void clientStart()
    {
        client->start();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::shared_ptr<Server> server;
    std::shared_ptr<Client> client;

    nexilis::server::ServerConfig settings;
    nexilis::client::ClientConfig server_data;
    std::unique_ptr<nexilis::client::ClientAPI> api;
};

template <typename Server, typename Client>
class ProtocolTestBoostTCP : public ProtocolTest<Server, Client>
{
protected:
    void setup() override
    {
        this->server_data.setBoostTCPAddress("127.0.0.1");
    }
};

template <typename Server, typename Client>
class ProtocolTestBoostUDP : public ProtocolTest<Server, Client>
{
protected:
    void setup() override
    {
        this->server_data.setBoostUDP("127.0.0.1");
    }
};

template <typename ProtocolTestType>
class ProtocolBasicTest : public ProtocolTestType
{
protected:
    void defaultSetup() override
    {
        this->setup();
        this->createSettings();
        this->createServer();
        this->serverStart();
        this->createDefaultServerData();
        this->createClientAPI();
        this->createClient();
    }
};

using BasicBoostTCPTest = ProtocolBasicTest<ProtocolTestBoostTCP<
        nexilis::server::nxboost::TCPServer, nexilis::client::nxboost::TCPClient>>;

TEST_F(BasicBoostTCPTest, ProtocolTestBoostTCPClientConnected)
{
    this->clientStart();

    waitFor(5, client->isConnected());
    EXPECT_TRUE(client->isConnected());
}

TEST_F(BasicBoostTCPTest, ProtocolTestBoostTCPHasActiveConnections)
{
    this->clientStart();

    waitFor(5, server->hasActiveConnections());
    EXPECT_TRUE(server->hasActiveConnections());
}

TEST_F(BasicBoostTCPTest, ProtocolTestBoostTCPActiveConnectionsCountFromOne)
{
    this->clientStart();

    waitFor(5, server->activeConnectionsCount() == 1);
    EXPECT_EQ(server->activeConnectionsCount(), 1);
}

TEST_F(BasicBoostTCPTest, ProtocolTestBoostTCPCreateRoom)
{
    this->clientStart();

    EXPECT_EQ(api->getActiveRooms().size(), 0);

    auto create_room = nexilis::client::Packet::Room::Management::create(*api, "room_mayn");

    this->client->sendMessage(create_room);
    waitFor(5, api->getActiveRooms().size() == 1);

    EXPECT_EQ(api->getActiveRooms().size(), 1);
}

TEST_F(BasicBoostTCPTest, ProtocolTestBoostTCPJoinRoom)
{
    this->clientStart();

    EXPECT_EQ(api->getActiveRooms().size(), 0);

    auto create_room = nexilis::client::Packet::Room::Management::create(
            *api, "room1", nexilis::RoomData::Context::_3D, 2);

    this->client->sendMessage(create_room);
    waitFor(5, api->getActiveRooms().size() == 1);

    EXPECT_EQ(api->getActiveRooms().size(), 1);
    auto room = api->getActiveRooms()[0];

    EXPECT_EQ(room.getClients().size(), 0);
    EXPECT_EQ(room.getMaxSize(), 2);
    auto room_id = api->getActiveRooms()[0].getId();
    auto join_room = nexilis::client::Packet::Room::Management::join(*api, room_id);
    this->client->sendMessage(join_room);

    waitRoomInfo(this->client, this->server, this->api);

    EXPECT_EQ(api->getActiveRooms().size(), 1);
    EXPECT_EQ(api->getActiveRooms()[0].getClients().size(), 1);
}

TEST_F(BasicBoostTCPTest, ProtocolTestBoostTCPMultipleRoomContexts)
{
    ASSERT_FALSE(client->isConnected());
    ASSERT_FALSE(server->hasActiveConnections());
    this->clientStart();

    // Create a 2D room
    this->client->sendMessage(nexilis::client::Packet::Room::Management::create(*api, "2D_room", nexilis::RoomData::Context::_2D));
    waitRoomInfo(this->client, this->server, this->api);

    EXPECT_EQ(api->getActiveRooms().size(), 1);

    // Create a 3D room
    this->client->sendMessage(nexilis::client::Packet::Room::Management::create(*api, "3D_room", nexilis::RoomData::Context::_3D));
    waitRoomInfo(this->client, this->server, this->api);

    EXPECT_EQ(api->getActiveRooms().size(), 2);

    // Verify room contexts
    auto rooms = api->getActiveRooms();
    EXPECT_EQ(rooms[0].getContext(), nexilis::RoomData::Context::_2D);
    EXPECT_EQ(rooms[1].getContext(), nexilis::RoomData::Context::_3D);
}

using BasicBoostUDPTest = ProtocolTestBoostTCP<nexilis::server::nxboost::UDPServer,
                                               nexilis::client::nxboost::UDPClient>;

template <typename ProtocolTestType, nexilis::RoomData::Context RoomContext>
class ProtocolRoomTest : public ProtocolTestType
{
protected:
    void defaultSetup() override
    {
        this->setup();
        this->createSettings();
        this->createServer();

        auto room = nexilis::server::Room(nexilis::RoomData(
                0,
                "RoomTestRoom",
                nexilis::Util::getRandomUint64(),
                RoomContext));
        auto id = room.getId();
        nexilis::server::RoomStorage::add(std::move(room));
        EXPECT_TRUE(nexilis::server::RoomStorage::contains(id));
        EXPECT_TRUE(nexilis::server::RoomStorage::getRoomById(id) != nullptr);

        this->serverStart();
        this->createDefaultServerData();
        this->createClientAPI();
        this->createClient();
    }
};

using RoomBoostTCP2DTest = ProtocolRoomTest<BasicBoostTCPTest, nexilis::RoomData::Context::_2D>;

TEST_F(RoomBoostTCP2DTest, ProtocolTestBoostTCPRoomClientConnected)
{
    this->clientStart();

    waitFor(5, client->isConnected() && server->hasActiveConnections());

    EXPECT_TRUE(client->isConnected());
    EXPECT_TRUE(server->hasActiveConnections());
    EXPECT_EQ(server->activeConnectionsCount(), 1);
}

TEST_F(RoomBoostTCP2DTest, ProtocolTestBoostTCPRoomCreation)
{
    ASSERT_FALSE(client->isConnected());
    ASSERT_FALSE(server->hasActiveConnections());
    this->clientStart();

    waitRoomInfo(this->client, this->server, this->api);
    EXPECT_EQ(api->getActiveRooms().size(), 1);

    this->client->sendMessage(nexilis::client::Packet::Room::Management::create(*api, "test"));
    waitRoomInfo(this->client, this->server, this->api);

    EXPECT_EQ(api->getActiveRooms().size(), 2);
}

TEST_F(RoomBoostTCP2DTest, ProtocolTestBoostTCPRoomInfoRooms)
{
    // Fresh state.
    ASSERT_FALSE(client->isConnected());
    ASSERT_FALSE(server->hasActiveConnections());

    // Start client.
    this->clientStart();

    // Use the updateRooms function to handle room updates
    waitRoomInfo(this->client, this->server, this->api);

    EXPECT_EQ(api->getActiveRooms().size(), 1);
}

// Test for room deletion
TEST_F(RoomBoostTCP2DTest, ProtocolTestBoostTCPRoomDelete)
{
    ASSERT_FALSE(client->isConnected());
    ASSERT_FALSE(server->hasActiveConnections());
    this->clientStart();

    waitRoomInfo(this->client, this->server, this->api);
    // Initially should have one room from setup
    EXPECT_EQ(api->getActiveRooms().size(), 1);

    // Create a new room
    this->client->sendMessage(nexilis::client::Packet::Room::Management::create(*api, "test_room"));
    waitRoomInfo(this->client, this->server, this->api);

    EXPECT_EQ(api->getActiveRooms().size(), 2);

    // Delete the created room after opting in to client-requested removal.
    auto room_id = api->getActiveRooms()[1].getId();
    auto room = nexilis::server::RoomStorage::getRoomById(room_id);
    ASSERT_NE(room, nullptr);
    room->setClientRoomRemovalAllowed(true);
    this->client->sendMessage(nexilis::client::Packet::Room::Management::remove(*api, room_id));
    waitRoomInfo(this->client, this->server, this->api);

    // Should be back to one room
    EXPECT_EQ(api->getActiveRooms().size(), 1);
}

// Test for room joining with invalid room ID
TEST_F(RoomBoostTCP2DTest, ProtocolTestBoostTCPJoinRoomInvalid)
{
    ASSERT_FALSE(client->isConnected());
    ASSERT_FALSE(server->hasActiveConnections());
    this->clientStart();

    // Try to join a non-existent room
    auto invalid_room_id = static_cast<uint64_t>(999999);
    auto join_room = nexilis::client::Packet::Room::Management::join(*api, invalid_room_id);
    this->client->sendMessage(join_room);

    // Should not crash, but should fail gracefully
    // We'll just verify the client doesn't crash
    EXPECT_TRUE(client->isConnected());
}

// Test for client leaving room
TEST_F(RoomBoostTCP2DTest, ProtocolTestBoostTCPLeaveRoom)
{
    ASSERT_FALSE(client->isConnected());
    ASSERT_FALSE(server->hasActiveConnections());
    this->clientStart();

    // Create a new room
    this->client->sendMessage(nexilis::client::Packet::Room::Management::create(*api, "test_room"));
    waitRoomInfo(this->client, this->server, this->api);

    EXPECT_EQ(api->getActiveRooms().size(), 2);

    // Join the new room
    auto room_id = api->getActiveRooms()[1].getId();
    auto join_room = nexilis::client::Packet::Room::Management::join(*api, room_id);
    this->client->sendMessage(join_room);
    waitRoomInfo(this->client, this->server, this->api);

    // Should be in the room now
    EXPECT_TRUE(api->clientInRoom());
    EXPECT_EQ(api->clientRoomId(), room_id);

    // Leave the room
    this->client->sendMessage(nexilis::client::Packet::Room::Management::leave(*api));
    waitRoomInfo(this->client, this->server, this->api);

    // Should no longer be in a room
    EXPECT_FALSE(api->clientInRoom());
}
