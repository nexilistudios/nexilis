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
#include <nexilis/command_type.hh>
#include <nexilis/logger/log.hh>
#include <nexilis/message_auth.hh>
#include <nexilis/ports.hh>
#include <nexilis/protocol_manager.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/room_storage.hh>

#include <nexilis/client/protocol/nxboost/tcp_client.hh>
#include <nexilis/server/protocol/nxboost/tcp_server.hh>
#include <nexilis/tls.hh>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <thread>

#include <boost/asio/connect.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/write.hpp>

class BoostTCPTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nexilis::Log::startConsoleDebugging();

        settings.setMode(nexilis::server::AuthenticationMode::password_protected);
        settings.setPassphrase("salasana");
        settings.setRootPassword("root");
        const auto identity = nexilis::tls::generateSelfSignedIdentity();
        ASSERT_FALSE(identity.publicKeySha256.empty());
        settings.setTlsIdentity(identity.certificatePem, identity.privateKeyPem);

        server = std::make_shared<nexilis::server::nxboost::TCPServer>(
                protocol_manager.createProtocol<nexilis::server::nxboost::TCPServer>(settings));
        server->start();

        std::this_thread::sleep_for(std::chrono::seconds(1));

        nexilis::client::ClientConfig server_data;
        server_data.setPassword("salasana");
        server_data.setBoostTCPAddress("127.0.0.1");
        server_data.setMode(nexilis::server::AuthenticationMode::password_protected);
        server_data.setServerPublicKeySha256(identity.publicKeySha256);

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

    void restartWithWorkerLimits(std::size_t pending, std::size_t total)
    {
        client->stop();
        server->stop();
        server.reset();
        nexilis::server::ClientStorage::clear();
        settings.setMode(nexilis::server::AuthenticationMode::skip);
        settings.setTls(false);
        settings.setMaxPendingBoostTcpConnections(pending);
        settings.setMaxBoostTcpConnections(total);
        server = std::make_shared<nexilis::server::nxboost::TCPServer>(
                protocol_manager.createProtocol<nexilis::server::nxboost::TCPServer>(settings));
        server->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    bool waitForWorkers(uint64_t active, std::size_t retained)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (server->activeConnectionsCount() == active && server->workerCount() == retained)
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return server->activeConnectionsCount() == active && server->workerCount() == retained;
    }

    bool waitForPeerClose(boost::asio::ip::tcp::socket& socket)
    {
        socket.non_blocking(true);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (std::chrono::steady_clock::now() < deadline)
        {
            char byte;
            boost::system::error_code ec;
            socket.read_some(boost::asio::buffer(&byte, 1), ec);
            if (ec == boost::asio::error::eof || ec == boost::asio::error::connection_reset)
                return true;
            if (ec && ec != boost::asio::error::would_block &&
                ec != boost::asio::error::try_again)
                return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
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

TEST_F(BoostTCPTest, PendingWorkerLimitRejectsAndReapsIdleHandshakes)
{
    restartWithWorkerLimits(1, 2);
    boost::asio::io_context io;
    const boost::asio::ip::tcp::endpoint endpoint(
            boost::asio::ip::make_address("127.0.0.1"), nexilis::Ports::getBoostTCPPort());
    boost::asio::ip::tcp::socket first(io);
    first.connect(endpoint);
    ASSERT_TRUE(waitForWorkers(1, 1));

    boost::asio::ip::tcp::socket rejected(io);
    rejected.connect(endpoint);
    EXPECT_TRUE(waitForPeerClose(rejected));
    EXPECT_TRUE(waitForWorkers(1, 1));
    rejected.close();
    first.close();
    ASSERT_TRUE(waitForWorkers(0, 0));

    boost::asio::ip::tcp::socket next(io);
    next.connect(endpoint);
    EXPECT_TRUE(waitForWorkers(1, 1));
    next.close();
    EXPECT_TRUE(waitForWorkers(0, 0));
}

TEST_F(BoostTCPTest, TotalWorkerLimitRejectsAdditionalHandshakes)
{
    restartWithWorkerLimits(2, 1);
    boost::asio::io_context io;
    const boost::asio::ip::tcp::endpoint endpoint(
            boost::asio::ip::make_address("127.0.0.1"), nexilis::Ports::getBoostTCPPort());
    boost::asio::ip::tcp::socket first(io);
    first.connect(endpoint);
    ASSERT_TRUE(waitForWorkers(1, 1));

    boost::asio::ip::tcp::socket rejected(io);
    rejected.connect(endpoint);
    EXPECT_TRUE(waitForPeerClose(rejected));
    EXPECT_TRUE(waitForWorkers(1, 1));
    rejected.close();
    first.close();
    EXPECT_TRUE(waitForWorkers(0, 0));
}

TEST_F(BoostTCPTest, BackToBackTlsRequestsReceiveBothResponses)
{
    client->start();
    ASSERT_TRUE(client->isConnected());

    client->sendMessageAsync(nexilis::client::Packet::Set::General::username(*api, "Alice")).get();
    client->sendMessageAsync(nexilis::client::Packet::Room::Management::create(*api, "chat")).get();

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (api->getActiveRooms().empty() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_FALSE(api->getActiveRooms().empty());
}

TEST_F(BoostTCPTest, PasswordModeRejectsPlaintextClient)
{
    nexilis::client::ClientConfig config;
    config.setMode(nexilis::server::AuthenticationMode::password_protected);
    config.setPassword("salasana");
    config.setBoostTCPAddress("127.0.0.1");
    config.setTls(false);
    nexilis::client::ClientAPI plaintextApi(config);
    EXPECT_THROW(nexilis::client::nxboost::TCPClient plaintextClient(plaintextApi),
                 std::runtime_error);
}

TEST_F(BoostTCPTest, LaterTlsSettingCannotDowngradePasswordConnection)
{
    api->setTls(false);
    client->start();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!client->isConnected() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_TRUE(client->isConnected());
}

TEST_F(BoostTCPTest, SecondSessionSocketCannotClaimActiveUser)
{
    startClientAndConnect();
    ASSERT_TRUE(client->isConnected());
    const auto userId = api->getClientId();
    auto user = nexilis::server::ClientStorage::getClientById(userId);
    ASSERT_NE(user, nullptr);
    auto firstRequest = nexilis::client::Packet::Room::Management::create(*api, "owner_room");
    client->sendMessageAsync(firstRequest).get();
    const auto claimDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while ((!user->hasTransport(nexilis::server::User::Transport::boostTcp) ||
            nexilis::server::RoomStorage::getAllRooms().empty()) &&
           std::chrono::steady_clock::now() < claimDeadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(user->hasTransport(nexilis::server::User::Transport::boostTcp));
    const auto roomCount = nexilis::server::RoomStorage::getAllRooms().size();
    ASSERT_EQ(roomCount, 1u);

    boost::asio::io_context io;
    boost::asio::ssl::context context(boost::asio::ssl::context::tls_client);
    context.set_verify_mode(boost::asio::ssl::verify_none);
    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> intruder(io, context);
    const auto baselineConnections = server->activeConnectionsCount();
    intruder.lowest_layer().connect({boost::asio::ip::make_address("127.0.0.1"), server->getPort()});
    intruder.handshake(boost::asio::ssl::stream_base::client);

    const auto request = nexilis::client::Packet::Room::Management::create(*api, "intruder_room");
    const auto signedRequest = nexilis::message_auth::sign(request, api->getUdpToken());
    ASSERT_FALSE(signedRequest.empty());
    const auto framed = nexilis::client::ClientProtocol::frame(signedRequest);
    boost::asio::write(intruder, boost::asio::buffer(framed));

    const auto rejectionDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (server->activeConnectionsCount() > baselineConnections &&
           std::chrono::steady_clock::now() < rejectionDeadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(server->activeConnectionsCount(), baselineConnections);
    EXPECT_EQ(nexilis::server::RoomStorage::getAllRooms().size(), roomCount);
    EXPECT_TRUE(user->hasTransport(nexilis::server::User::Transport::boostTcp));
    EXPECT_TRUE(client->isConnected());
}

TEST_F(BoostTCPTest, StolenSessionTokenCannotClaimPendingSwitch)
{
    startClientAndConnect();
    auto victim = std::make_shared<nexilis::server::User>(nexilis::Util::getRandomUint64(),
                                                          "127.0.0.1");
    victim->setCommonAccess(true);
    victim->beginPendingHandshake(std::chrono::steady_clock::now());
    ASSERT_TRUE(nexilis::server::ClientStorage::add(victim));

    boost::asio::io_context io;
    boost::asio::ssl::context context(boost::asio::ssl::context::tls_client);
    context.set_verify_mode(boost::asio::ssl::verify_none);
    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> intruder(io, context);
    const auto baselineConnections = server->activeConnectionsCount();
    intruder.lowest_layer().connect({boost::asio::ip::make_address("127.0.0.1"), server->getPort()});
    intruder.handshake(boost::asio::ssl::stream_base::client);

    auto request = nexilis::Util::convertToByteVector(victim->getId());
    const auto messageId = nexilis::Util::convertToByteVector(uint64_t{1});
    request.insert(request.end(), messageId.begin(), messageId.end());
    request.insert(request.end(), {static_cast<uint8_t>(nexilis::CommandType::getting), 0, 0});
    const auto signedRequest = nexilis::message_auth::sign(request, victim->getUdpToken());
    ASSERT_FALSE(signedRequest.empty());
    const auto framed = nexilis::client::ClientProtocol::frame(signedRequest);
    boost::asio::write(intruder, boost::asio::buffer(framed));

    const auto rejectionDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (server->activeConnectionsCount() > baselineConnections &&
           std::chrono::steady_clock::now() < rejectionDeadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(server->activeConnectionsCount(), baselineConnections);
    EXPECT_FALSE(victim->hasTransport(nexilis::server::User::Transport::boostTcp));
    EXPECT_FALSE(victim->getBoostTcpSwitchToken().empty());
}

TEST_F(BoostTCPTest, RejectsWrongServerPinBeforeAuthentication)
{
    nexilis::client::ClientConfig wrongConfig;
    wrongConfig.setMode(nexilis::server::AuthenticationMode::password_protected);
    wrongConfig.setPassword("salasana");
    wrongConfig.setBoostTCPAddress("127.0.0.1");
    wrongConfig.setServerPublicKeySha256(std::string(64, '0'));
    nexilis::client::ClientAPI wrongApi(wrongConfig);
    nexilis::client::nxboost::TCPClient wrongClient(wrongApi);

    wrongClient.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_FALSE(wrongClient.isConnected());
    EXPECT_EQ(wrongApi.getClientId(), 0u);
    wrongClient.stop();
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
    settings.setMode(nexilis::server::AuthenticationMode::skip);
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

    nexilis::nx_data frame(4 + 19, 0);
    const uint32_t size = htonl(19);
    std::memcpy(frame.data(), &size, sizeof(size));
    frame[4 + 16] = static_cast<uint8_t>(nexilis::CommandType::getting);
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

TEST_F(BoostTCPTest, PlaintextSkipModeSwitchesWithOneTimeCredential)
{
    client->stop();
    server->stop();
    server.reset();
    nexilis::server::ClientStorage::clear();

    settings.setMode(nexilis::server::AuthenticationMode::skip);
    settings.setTls(false);
    server = std::make_shared<nexilis::server::nxboost::TCPServer>(
            protocol_manager.createProtocol<nexilis::server::nxboost::TCPServer>(settings));
    server->start();

    nexilis::client::ClientConfig config;
    config.setMode(nexilis::server::AuthenticationMode::skip);
    config.setBoostTCPAddress("127.0.0.1");
    config.setTls(false);
    api = std::make_unique<nexilis::client::ClientAPI>(config);
    client = std::make_shared<nexilis::client::nxboost::TCPClient>(
            protocol_manager.createProtocol<nexilis::client::nxboost::TCPClient>(*api));
    client->start();

    const auto readyDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!client->isConnected() && std::chrono::steady_clock::now() < readyDeadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(client->isConnected());
    auto user = nexilis::server::ClientStorage::getClientById(api->getClientId());
    ASSERT_NE(user, nullptr);
    EXPECT_TRUE(user->hasTransport(nexilis::server::User::Transport::boostTcp));

    client->sendMessageAsync(nexilis::client::Packet::Room::Management::create(*api, "skip_room")).get();
    const auto roomDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (nexilis::server::RoomStorage::getAllRooms().empty() &&
           std::chrono::steady_clock::now() < roomDeadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(nexilis::server::RoomStorage::getAllRooms().size(), 1u);
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
