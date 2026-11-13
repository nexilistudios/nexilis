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
#include <nexilis/udp_auth.hh>

#include <nexilis/client/protocol/af_inet/udp_client.hh>
#include <nexilis/server/protocol/af_inet/udp_server.hh>

#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <future>
#include <stdexcept>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

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
        nexilis::server::RoomStorage::clear();
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

TEST_F(AFInetUDPTest, RejectsClientIdFromAnotherEndpoint)
{
    startClientAndConnect();
    ASSERT_FALSE(api->getUdpToken().empty());

    auto before = nexilis::server::RoomStorage::getAllRooms().size();
    auto beforeClients = nexilis::server::ClientStorage::getAllClients().size();
    auto command = nexilis::client::Packet::Room::Management::create(*api, "forged_inet_room");
    int attacker = socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(attacker, 0);
    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(port);
    ASSERT_EQ(inet_pton(AF_INET, "127.0.0.1", &target.sin_addr), 1);

    auto send = [&](const nexilis::nx_data& packet)
    {
        EXPECT_EQ(sendto(attacker, packet.data(), packet.size(), 0,
                         reinterpret_cast<sockaddr*>(&target), sizeof(target)),
                  static_cast<ssize_t>(packet.size()));
    };
    send(command);
    send(nexilis::udp_auth::makeSession(command, std::string(32, '0')));
    send(nexilis::udp_auth::makeSession(command, api->getUdpToken()));
    nexilis::nx_data badCookie(nexilis::udp_auth::cookie_size, 0);
    nexilis::nx_data password{'s', 'a', 'l', 'a', 's', 'a', 'n', 'a'};
    send(nexilis::udp_auth::makeAuthentication(badCookie, password));
    close(attacker);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(nexilis::server::RoomStorage::getAllRooms().size(), before);
    EXPECT_EQ(nexilis::server::ClientStorage::getAllClients().size(), beforeClients);
}

namespace
{
struct UDPTestPeer
{
    UDPTestPeer()
        : fd(socket(AF_INET, SOCK_DGRAM, 0))
    {
    }
    ~UDPTestPeer()
    {
        if (fd != -1)
            close(fd);
    }
    UDPTestPeer(const UDPTestPeer&) = delete;
    UDPTestPeer& operator=(const UDPTestPeer&) = delete;

    std::shared_ptr<nexilis::server::User> authenticate(uint16_t port)
    {
        sockaddr_in target{};
        target.sin_family = AF_INET;
        target.sin_port = htons(port);
        target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        timeval timeout{};
        timeout.tv_sec = 2;
        if (fd == -1 || setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1)
            return {};
        const auto send = [&](const nexilis::nx_data& bytes)
        {
            return sendto(fd, bytes.data(), bytes.size(), 0,
                          reinterpret_cast<const sockaddr*>(&target), sizeof(target)) == static_cast<ssize_t>(bytes.size());
        };
        if (!send(nexilis::udp_auth::makeHello()))
            return {};
        nexilis::nx_data response(1024);
        auto received = recv(fd, response.data(), response.size(), 0);
        if (received != 4 + nexilis::udp_auth::cookie_size ||
            !nexilis::udp_auth::hasPrefix(response, nexilis::udp_auth::challenge))
            return {};
        nexilis::nx_data cookie(response.begin() + 4, response.begin() + received);
        if (!send(nexilis::udp_auth::makeAuthentication(cookie, {'s', 'a', 'l', 'a', 's', 'a', 'n', 'a'})))
            return {};
        // The authentication reply follows callback installation.
        if (recv(fd, response.data(), response.size(), 0) <= 0)
            return {};
        const auto users = nexilis::server::ClientStorage::getClientsByIpAddress("127.0.0.1");
        return users.size() == 1 ? users.front() : nullptr;
    }

    int fd;
};

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

TEST_F(AFInetUDPTest, IdleDatagramTransportRemovesUserAndLeavesRoom)
{
    // Recreate the server with a short idle timeout so the test does not wait
    // for the production default.
    server->stop();
    server.reset();
    nexilis::server::ClientStorage::clear();
    nexilis::server::RoomStorage::clear();

    settings.setDatagramIdleTimeout(std::chrono::milliseconds(500));
    server = std::make_shared<nexilis::server::af_inet::UDPServer>(
            protocol_manager.createProtocol<nexilis::server::af_inet::UDPServer>(settings, port));
    server->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    UDPTestPeer peer;
    const auto user = peer.authenticate(port);
    ASSERT_NE(user, nullptr);
    const auto userId = user->getId();

    const auto roomId = attachRoom(user, "idle_inet_udp_room");
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

TEST_F(AFInetUDPTest, RetainedCallbackRejectsAfterStopDestructionAndReplacement)
{
    UDPTestPeer peer;
    const auto retained = peer.authenticate(port);
    ASSERT_NE(retained, nullptr);
    ASSERT_TRUE(retained->inetUDPSend({42}));
    char response[1024];
    ssize_t received;
    do
    {
        received = recv(peer.fd, response, sizeof(response), 0);
    } while (received > 1);
    ASSERT_EQ(received, 1);
    EXPECT_EQ(response[0], 42);
    server->stop();
    EXPECT_THROW(retained->inetUDPSend({42}), std::runtime_error);
    server.reset();
    EXPECT_THROW(retained->inetUDPSend({42}), std::runtime_error);

    nexilis::server::ClientStorage::clear();
    server = std::make_shared<nexilis::server::af_inet::UDPServer>(settings, port);
    server->start();
    ASSERT_NE(peer.authenticate(port), nullptr);
    EXPECT_THROW(retained->inetUDPSend({42}), std::runtime_error);
}

TEST_F(AFInetUDPTest, StopAndDestructionSerializeWithConcurrentCallbackSends)
{
    UDPTestPeer peer;
    const auto retained = peer.authenticate(port);
    ASSERT_NE(retained, nullptr);
    std::promise<void> sending;
    auto started = sending.get_future();
    std::atomic<unsigned> sent{0};
    std::jthread sender([&](std::stop_token stop)
                        {
                            try
                            {
                                retained->inetUDPSend({42});
                                ++sent;
                            }
                            catch (...)
                            {
                            }
                            sending.set_value();
                            while (!stop.stop_requested())
                            {
                                try
                                {
                                    retained->inetUDPSend({42});
                                    ++sent;
                                }
                                catch (const std::runtime_error&)
                                {
                                }
                            } });
    started.wait();
    EXPECT_GT(sent.load(), 0u);
    const auto before = std::chrono::steady_clock::now();
    server.reset();
    EXPECT_LT(std::chrono::steady_clock::now() - before, std::chrono::seconds(1));
    EXPECT_THROW(retained->inetUDPSend({42}), std::runtime_error);
    sender.request_stop();
}

TEST_F(AFInetUDPTest, MoveAssignmentClosesOldCallbacksAndRejectsRunningSource)
{
    UDPTestPeer peer;
    const auto retained = peer.authenticate(port);
    ASSERT_NE(retained, nullptr);
    // The receiver captures its owner, so moving a running source is rejected.
    EXPECT_THROW(nexilis::server::af_inet::UDPServer(std::move(*server)), std::logic_error);
    nexilis::server::af_inet::UDPServer replacement(settings, port + 1);
    replacement.start();
    EXPECT_THROW(*server = std::move(replacement), std::logic_error);
    replacement.stop();
    nexilis::server::af_inet::UDPServer unstarted(settings, port + 1);
    *server = std::move(unstarted);
    EXPECT_THROW(retained->inetUDPSend({42}), std::runtime_error);
    server->start();
    nexilis::server::ClientStorage::clear();
    ASSERT_NE(peer.authenticate(port + 1), nullptr);
}
