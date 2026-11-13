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
#include <nexilis/protocol_manager.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/room_storage.hh>
#include <nexilis/udp_auth.hh>

#include <boost/asio/ip/udp.hpp>

#include <nexilis/client/protocol/nxboost/udp_client.hh>
#include <nexilis/server/protocol/nxboost/udp_server.hh>

class BoostUDPTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nexilis::Log::startConsoleDebugging();

        settings.setMode(nexilis::server::AuthenticationMode::password_protected);
        settings.setPassphrase("salasana");
        settings.setRootPassword("root");

        server = std::make_shared<nexilis::server::nxboost::UDPServer>(
                protocol_manager.createProtocol<nexilis::server::nxboost::UDPServer>(settings));
        server->start();

        std::this_thread::sleep_for(std::chrono::seconds(1));

        nexilis::client::ClientConfig server_data;
        server_data.setPassword("salasana");
        server_data.setBoostUDPAddress("localhost");
        server_data.setMode(nexilis::server::AuthenticationMode::password_protected);

        api = std::make_unique<nexilis::client::ClientAPI>(server_data);
        client = std::make_shared<nexilis::client::nxboost::UDPClient>(
                protocol_manager.createProtocol<nexilis::client::nxboost::UDPClient>(*api));
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

    nexilis::ProtocolManager protocol_manager;
    nexilis::server::ServerConfig settings;

    std::unique_ptr<nexilis::client::ClientAPI> api;

    std::shared_ptr<nexilis::server::nxboost::UDPServer> server;
    std::shared_ptr<nexilis::client::nxboost::UDPClient> client;
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

TEST_F(BoostUDPTest, SendMessageAsyncReturnsReadyFuture)
{
    client->start();

    const auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!client->isConnected() && std::chrono::steady_clock::now() < timeout)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ASSERT_TRUE(client->isConnected());

    auto payload = nexilis::client::Packet::Room::Management::create(*api, "async_room");
    auto future = client->sendMessageAsync(payload);

    auto status = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(status, std::future_status::ready);

    EXPECT_NO_THROW(future.get());
}

TEST_F(BoostUDPTest, SendMessageAsyncReturnsReadyFutureOnStoppedClient)
{
    // Don't start the client.
    auto payload = nexilis::client::Packet::Room::Management::create(*api, "test");
    auto future = client->sendMessageAsync(payload);

    auto status = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(status, std::future_status::ready);

    // Should throw since the client is stopped.
    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST_F(BoostUDPTest, SendMessageAsyncReturnsReadyFutureOnClosedSocket)
{
    client->start();

    const auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!client->isConnected() && std::chrono::steady_clock::now() < timeout)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ASSERT_TRUE(client->isConnected());

    client->stop();

    auto payload = nexilis::client::Packet::Room::Management::create(*api, "test");
    auto future = client->sendMessageAsync(payload);

    auto status = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(status, std::future_status::ready);

    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST_F(BoostUDPTest, SendMessageAsyncDeliversMessage)
{
    client->start();

    const auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!client->isConnected() && std::chrono::steady_clock::now() < timeout)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ASSERT_TRUE(client->isConnected());

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

TEST_F(BoostUDPTest, RejectsClientIdFromAnotherEndpoint)
{
    client->start();
    ASSERT_TRUE(client->isConnected());
    ASSERT_FALSE(api->getUdpToken().empty());

    auto before = nexilis::server::RoomStorage::getAllRooms().size();
    auto beforeClients = nexilis::server::ClientStorage::getAllClients().size();
    auto command = nexilis::client::Packet::Room::Management::create(*api, "forged_boost_room");

    boost::asio::io_context context;
    boost::asio::ip::udp::socket attacker(context, boost::asio::ip::udp::v4());
    auto target = boost::asio::ip::udp::endpoint(
            boost::asio::ip::make_address_v4("127.0.0.1"), server->getPort());

    // A bare client ID is never enough, including from a different socket on
    // the same IP. A captured token must not permit rebinding the endpoint.
    attacker.send_to(boost::asio::buffer(command), target);
    auto wrongToken = nexilis::udp_auth::makeSession(command, std::string(32, '0'));
    attacker.send_to(boost::asio::buffer(wrongToken), target);
    auto stolenToken = nexilis::udp_auth::makeSession(command, api->getUdpToken());
    attacker.send_to(boost::asio::buffer(stolenToken), target);
    nexilis::nx_data badCookie(nexilis::udp_auth::cookie_size, 0);
    nexilis::nx_data password{'s', 'a', 'l', 'a', 's', 'a', 'n', 'a'};
    auto fakeAuthentication = nexilis::udp_auth::makeAuthentication(badCookie, password);
    attacker.send_to(boost::asio::buffer(fakeAuthentication), target);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(nexilis::server::RoomStorage::getAllRooms().size(), before);
    EXPECT_EQ(nexilis::server::ClientStorage::getAllClients().size(), beforeClients);
}

TEST_F(BoostUDPTest, ReplayedSignedCommandRunsOnce)
{
    client->start();
    ASSERT_TRUE(client->isConnected());

    auto before = nexilis::server::RoomStorage::getAllRooms().size();
    auto command = nexilis::client::Packet::Room::Management::create(*api, "replay_room");
    client->sendMessage(command);
    client->sendMessage(command);

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_EQ(nexilis::server::RoomStorage::getAllRooms().size(), before + 1);
}

TEST_F(BoostUDPTest, MalformedCommandDoesNotStopSubsequentDatagrams)
{
    client->start();
    ASSERT_TRUE(client->isConnected());
    const auto before = nexilis::server::RoomStorage::getRoomCount();
    auto malformed = nexilis::client::Packet::Room::Management::join(*api, 1);
    malformed.resize(16 + 3); // Keep session/message IDs and command, omit room ID.
    client->sendMessage(malformed);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    client->sendMessage(nexilis::client::Packet::Room::Management::create(*api, "after_error"));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (nexilis::server::RoomStorage::getRoomCount() == before &&
           std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(nexilis::server::RoomStorage::getRoomCount(), before + 1);
}

TEST_F(BoostUDPTest, IdleDatagramTransportRemovesUserAndLeavesRoom)
{
    // Recreate the server and client with a short idle timeout so the test does
    // not wait for the production default.
    client->stop();
    server->stop();
    server.reset();
    nexilis::server::ClientStorage::clear();
    nexilis::server::RoomStorage::clear();

    settings.setDatagramIdleTimeout(std::chrono::milliseconds(500));
    server = std::make_shared<nexilis::server::nxboost::UDPServer>(
            protocol_manager.createProtocol<nexilis::server::nxboost::UDPServer>(settings));
    server->start();

    nexilis::client::ClientConfig server_data;
    server_data.setPassword("salasana");
    server_data.setBoostUDPAddress("localhost");
    server_data.setMode(nexilis::server::AuthenticationMode::password_protected);

    api = std::make_unique<nexilis::client::ClientAPI>(server_data);
    client = std::make_shared<nexilis::client::nxboost::UDPClient>(
            protocol_manager.createProtocol<nexilis::client::nxboost::UDPClient>(*api));
    client->start();

    const auto connectDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!client->isConnected() && std::chrono::steady_clock::now() < connectDeadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_TRUE(client->isConnected());

    const auto userId = api->getClientId();
    auto user = nexilis::server::ClientStorage::getClientById(userId);
    ASSERT_NE(user, nullptr);

    const auto roomId = attachRoom(user, "idle_boost_udp_room");
    ASSERT_NE(roomId, 0u);
    auto room = nexilis::server::RoomStorage::getRoomById(roomId);
    ASSERT_NE(room, nullptr);
    ASSERT_TRUE(room->contains(userId));

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    while (nexilis::server::ClientStorage::contains(userId) &&
           std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_FALSE(nexilis::server::ClientStorage::contains(userId));
    EXPECT_FALSE(room->contains(userId));
}

TEST_F(BoostUDPTest, RetainedCallbackRejectsAfterStopDestructionAndReplacement)
{
    client->start();
    ASSERT_TRUE(client->isConnected());
    const auto retained = nexilis::server::ClientStorage::getClientById(api->getClientId());
    ASSERT_NE(retained, nullptr);
    ASSERT_TRUE(retained->isBoostUDPSet());
    server->stop();
    EXPECT_THROW(retained->boostUDPSend({42}), std::runtime_error);
    server.reset();
    EXPECT_THROW(retained->boostUDPSend({42}), std::runtime_error);
    server = std::make_shared<nexilis::server::nxboost::UDPServer>(settings);
    server->start();
    EXPECT_THROW(retained->boostUDPSend({42}), std::runtime_error);
}

TEST_F(BoostUDPTest, DoubleStartAndMovesPreserveLifecycle)
{
    EXPECT_NO_THROW(server->start());
    EXPECT_THROW(nexilis::server::nxboost::UDPServer(std::move(*server)), std::logic_error);
    client->start();
    ASSERT_TRUE(client->isConnected());
    const auto retained = nexilis::server::ClientStorage::getClientById(api->getClientId());
    ASSERT_NE(retained, nullptr);
    server->stop();
    nexilis::server::nxboost::UDPServer replacement(settings);
    replacement.start();
    EXPECT_THROW(*server = std::move(replacement), std::logic_error);
    replacement.stop();
    *server = std::move(replacement);
    EXPECT_THROW(retained->boostUDPSend({42}), std::runtime_error);
    // A stopped instance stays stopped after a move and a repeated start.
    EXPECT_NO_THROW(server->start());
    EXPECT_NO_THROW(server->stop());
}

TEST_F(BoostUDPTest, DestructionSerializesWithConcurrentCallbackSends)
{
    client->start();
    ASSERT_TRUE(client->isConnected());
    const auto retained = nexilis::server::ClientStorage::getClientById(api->getClientId());
    ASSERT_NE(retained, nullptr);
    client->stop();
    std::promise<void> sending;
    auto started = sending.get_future();
    std::jthread sender([&](std::stop_token stop)
                        {
                            sending.set_value();
                            while (!stop.stop_requested())
                            {
                                try
                                {
                                    retained->boostUDPSend({42});
                                }
                                catch (const std::runtime_error&)
                                {
                                }
                            } });
    started.wait();
    server.reset();
    EXPECT_THROW(retained->boostUDPSend({42}), std::runtime_error);
    sender.request_stop();
}
