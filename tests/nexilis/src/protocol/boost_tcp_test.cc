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
#include <nexilis/server/room_storage.hh>

#include <nexilis/client/protocol/nxboost/tcp_client.hh>
#include <nexilis/server/protocol/nxboost/tcp_server.hh>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <thread>

class BoostTCPTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nexilis::Log::startConsoleDebugging();

        settings.setMode(nexilis::server::AuthenticationMode::password_protected);
        settings.setPassphrase("salasana");
        settings.setRootPassword("root");

        server = std::make_shared<nexilis::server::nxboost::TCPServer>(
                protocol_manager.createProtocol<nexilis::server::nxboost::TCPServer>(settings));
        server->start();

        std::this_thread::sleep_for(std::chrono::seconds(1));

        nexilis::client::ClientConfig server_data;
        server_data.setPassword("salasana");
        server_data.setBoostTCPAddress("127.0.0.1");
        server_data.setMode(nexilis::server::AuthenticationMode::password_protected);

        api = std::make_unique<nexilis::client::ClientAPI>(server_data);
        client = std::make_shared<nexilis::client::nxboost::TCPClient>(
                protocol_manager.createProtocol<nexilis::client::nxboost::TCPClient>(*api));
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
        nexilis::server::RoomStorage::clear();
        nexilis::Log::stopLogging();
    }

    void startClientAndConnect()
    {
        client->start();

        const auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (server->activeConnectionsCount() == 0 && std::chrono::steady_clock::now() < timeout)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Wait extra time for port switch to complete.
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    nexilis::ProtocolManager protocol_manager;
    nexilis::server::ServerConfig settings;
    std::unique_ptr<nexilis::client::ClientAPI> api;
    std::shared_ptr<nexilis::server::nxboost::TCPServer> server;
    std::shared_ptr<nexilis::client::nxboost::TCPClient> client;
};

namespace
{
uint64_t attachRoom(const std::shared_ptr<nexilis::server::User>& user, const std::string& name)
{
    const auto roomId = nexilis::Util::getRandomUint64();
    if (!nexilis::server::RoomStorage::add(
                nexilis::server::Room(nexilis::RoomData(user->getId(), name, roomId))))
        return 0;

    auto room = nexilis::server::RoomStorage::getRoomById(roomId);
    if (!room)
        return 0;

    room->joinRoom(user->getId());
    user->setRoomId(roomId);
    return roomId;
}
} // namespace

TEST_F(BoostTCPTest, BoostTCPSuccessfulConnection)
{
    client->start();

    std::this_thread::sleep_for(std::chrono::seconds(5));

    EXPECT_TRUE(client->isConnected());
    EXPECT_TRUE(server->hasActiveConnections());
    EXPECT_EQ(server->activeConnectionsCount(), 1);
}

TEST_F(BoostTCPTest, AbandonedHandshakeIsRemovedOnStop)
{
    // Drive the handshake manually so the client never opens the session port.
    // A successful handshake that never opens a session leaves pending presence
    // that the server must clean up on stop.
    client->stop();
    server->stop();
    server.reset();
    nexilis::server::ClientStorage::clear();
    nexilis::server::RoomStorage::clear();

    settings.setTls(false);
    server = std::make_shared<nexilis::server::nxboost::TCPServer>(
            protocol_manager.createProtocol<nexilis::server::nxboost::TCPServer>(settings));
    server->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(fd, -1);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(nexilis::Ports::getBoostTCPPort());
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ASSERT_EQ(connect(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)), 0);

    timeval timeout{};
    timeout.tv_sec = 2;
    ASSERT_EQ(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)), 0);

    const std::string password = "salasana";
    nexilis::nx_data frame(4 + password.size());
    const uint32_t size = htonl(static_cast<uint32_t>(password.size()));
    std::memcpy(frame.data(), &size, sizeof(size));
    std::memcpy(frame.data() + 4, password.data(), password.size());
    ASSERT_EQ(send(fd, frame.data(), frame.size(), MSG_NOSIGNAL),
              static_cast<ssize_t>(frame.size()));

    // Drain the client-id and session-port replies. EOF means the handshake
    // worker finished with the session still unopened.
    uint8_t buffer[1024];
    while (recv(fd, buffer, sizeof(buffer), 0) > 0)
    {
    }
    close(fd);

    std::shared_ptr<nexilis::server::User> user;
    const auto userDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!user && std::chrono::steady_clock::now() < userDeadline)
    {
        const auto users = nexilis::server::ClientStorage::getAllClients();
        if (!users.empty())
            user = users.front();
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_NE(user, nullptr);
    EXPECT_TRUE(user->hasTransportPresence());
    const auto userId = user->getId();

    const auto roomId = attachRoom(user, "abandoned_handshake_room");
    ASSERT_NE(roomId, 0u);
    auto room = nexilis::server::RoomStorage::getRoomById(roomId);
    ASSERT_NE(room, nullptr);
    ASSERT_TRUE(room->contains(userId));

    server->stop();

    EXPECT_FALSE(nexilis::server::ClientStorage::contains(userId));
    EXPECT_FALSE(room->contains(userId));
}

TEST_F(BoostTCPTest, SendMessageAsyncReturnsReadyFuture)
{
    startClientAndConnect();

    auto payload = nexilis::client::Packet::Room::Management::create(*api, "async_room");
    auto future = client->sendMessageAsync(payload);

    auto status = future.wait_for(std::chrono::seconds(10));
    EXPECT_EQ(status, std::future_status::ready);

    // The future should be fulfilled (either value or exception from port switch).
    // Either way, get() should not hang.
    try
    {
        future.get();
    }
    catch (const std::exception& e)
    {
        // Acceptable if the port switch caused the write to fail.
        nexilis::Log::debug("SendMessageAsync threw (expected during port switch): ", e.what());
    }
}

TEST_F(BoostTCPTest, SendMessageAsyncReturnsReadyFutureOnStoppedClient)
{
    auto payload = nexilis::client::Packet::Room::Management::create(*api, "test");
    auto future = client->sendMessageAsync(payload);

    auto status = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(status, std::future_status::ready);

    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST_F(BoostTCPTest, SendMessageAsyncReturnsReadyFutureOnClosedSocket)
{
    startClientAndConnect();

    client->stop();

    auto payload = nexilis::client::Packet::Room::Management::create(*api, "test");
    auto future = client->sendMessageAsync(payload);

    auto status = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(status, std::future_status::ready);

    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST_F(BoostTCPTest, SendMessageAsyncDeliversMessage)
{
    startClientAndConnect();

    EXPECT_EQ(api->getActiveRooms().size(), 0);
    auto payload = nexilis::client::Packet::Room::Management::create(*api, "async_room");
    auto future = client->sendMessageAsync(payload);

    auto status = future.wait_for(std::chrono::seconds(10));
    ASSERT_EQ(status, std::future_status::ready);

    try
    {
        future.get();
    }
    catch (const std::exception& e)
    {
        nexilis::Log::debug("SendMessageAsync threw: ", e.what());
    }

    // Wait for server to process the message and create the room.
    const auto room_timeout = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (api->getActiveRooms().size() == 0 && std::chrono::steady_clock::now() < room_timeout)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    EXPECT_EQ(api->getActiveRooms().size(), 1);
}
