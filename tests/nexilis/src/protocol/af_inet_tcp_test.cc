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

#include <nexilis/client/protocol/af_inet/tcp_client.hh>
#include <nexilis/server/protocol/af_inet/tcp_server.hh>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <future>
#include <system_error>
#include <thread>

class AFInetTCPTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nexilis::Log::startConsoleDebugging();

        settings.setMode(nexilis::server::AuthenticationMode::password_protected);
        settings.setPassphrase("salasana");
        settings.setRootPassword("root");

        server = std::make_shared<nexilis::server::af_inet::TCPServer>(
                protocol_manager.createProtocol<nexilis::server::af_inet::TCPServer>(settings, port));
        server->start();

        std::this_thread::sleep_for(std::chrono::seconds(1));

        nexilis::client::ClientConfig server_data;
        server_data.setPassword("salasana");
        server_data.setInetTCP("127.0.0.1");
        server_data.setInetTCPServerPort(port);
        server_data.setMode(nexilis::server::AuthenticationMode::password_protected);

        api = std::make_unique<nexilis::client::ClientAPI>(server_data);
        client = std::make_shared<nexilis::client::af_inet::TCPClient>(
                protocol_manager.createProtocol<nexilis::client::af_inet::TCPClient>(*api));
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
        EXPECT_TRUE(server->hasActiveConnections());
        EXPECT_EQ(server->activeConnectionsCount(), 1);
    }

    nexilis::ProtocolManager protocol_manager;
    nexilis::server::ServerConfig settings;

    static constexpr uint16_t port = 55210;

    std::unique_ptr<nexilis::client::ClientAPI> api;
    std::shared_ptr<nexilis::server::af_inet::TCPServer> server;
    std::shared_ptr<nexilis::client::af_inet::TCPClient> client;
};

TEST_F(AFInetTCPTest, TCPClientConnectsToServer)
{
    startClientAndConnect();
}

TEST_F(AFInetTCPTest, SendMessageAsyncReturnsReadyFuture)
{
    startClientAndConnect();

    auto payload = nexilis::client::Packet::Room::Management::create(*api, "async_room");
    auto future = client->sendMessageAsync(payload);

    auto status = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(status, std::future_status::ready);

    EXPECT_NO_THROW(future.get());
}

TEST_F(AFInetTCPTest, SendMessageAsyncReturnsReadyFutureOnStoppedClient)
{
    // Don't start the client.
    auto payload = nexilis::client::Packet::Room::Management::create(*api, "test");
    auto future = client->sendMessageAsync(payload);

    auto status = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(status, std::future_status::ready);

    // Should throw since the client is stopped.
    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST_F(AFInetTCPTest, SendMessageAsyncReturnsReadyFutureOnClosedSocket)
{
    startClientAndConnect();

    client->stop();

    auto payload = nexilis::client::Packet::Room::Management::create(*api, "test");
    auto future = client->sendMessageAsync(payload);

    auto status = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(status, std::future_status::ready);

    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST_F(AFInetTCPTest, SendMessageAsyncDeliversMessage)
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

namespace
{
class TCPTestPeer
{
public:
    TCPTestPeer() : fd(socket(AF_INET, SOCK_STREAM, 0))
    {
    }

    ~TCPTestPeer()
    {
        disconnect();
    }

    void disconnect()
    {
        if (fd != -1)
        {
            shutdown(fd, SHUT_RDWR);
            close(fd);
            fd = -1;
        }
    }

    std::shared_ptr<nexilis::server::User> authenticate(uint16_t port)
    {
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        timeval timeout{};
        timeout.tv_sec = 2;
        if (fd == -1 || setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1 ||
            connect(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == -1)
            return {};
        const uint32_t size = htonl(8);
        nexilis::nx_data frame(sizeof(size));
        std::memcpy(frame.data(), &size, sizeof(size));
        const std::string password = "salasana";
        frame.insert(frame.end(), password.begin(), password.end());
        if (send(fd, frame.data(), frame.size(), MSG_NOSIGNAL) != static_cast<ssize_t>(frame.size()))
            return {};
        if (receive().empty())
            return {};
        const auto users = nexilis::server::ClientStorage::getClientsByIpAddress("127.0.0.1");
        return users.size() == 1 ? users.front() : nullptr;
    }

    nexilis::nx_data receive()
    {
        uint32_t size;
        if (recv(fd, &size, sizeof(size), MSG_WAITALL) != sizeof(size))
            return {};
        const auto length = ntohl(size);
        if (length > 4096)
            return {};
        nexilis::nx_data bytes(length);
        if (recv(fd, bytes.data(), bytes.size(), MSG_WAITALL) != static_cast<ssize_t>(bytes.size()))
            return {};
        return bytes;
    }

private:
    int fd;
};
} // namespace

TEST_F(AFInetTCPTest, RetainedCallbackRejectsAfterStopDestructionAndReplacement)
{
    TCPTestPeer peer;
    const auto retained = peer.authenticate(port);
    ASSERT_NE(retained, nullptr);
    const nexilis::nx_data payload{42, 43};
    EXPECT_TRUE(retained->inetTCPSend(payload));
    EXPECT_EQ(peer.receive(), payload);
    server->stop();
    EXPECT_EQ(server->activeConnectionsCount(), 0);
    EXPECT_THROW(retained->inetTCPSend(payload), std::runtime_error);
    server.reset();
    EXPECT_THROW(retained->inetTCPSend(payload), std::runtime_error);
    server = std::make_shared<nexilis::server::af_inet::TCPServer>(settings, port);
    server->start();
    nexilis::server::ClientStorage::clear();
    TCPTestPeer replacement;
    const auto current = replacement.authenticate(port);
    ASSERT_NE(current, nullptr);
    EXPECT_THROW(retained->inetTCPSend(payload), std::runtime_error);
    EXPECT_TRUE(current->inetTCPSend(payload));
    EXPECT_EQ(replacement.receive(), payload);
}

TEST_F(AFInetTCPTest, DisconnectDisablesRetainedCallback)
{
    TCPTestPeer peer;
    const auto retained = peer.authenticate(port);
    ASSERT_NE(retained, nullptr);
    peer.disconnect();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (server->hasActiveConnections() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_EQ(server->activeConnectionsCount(), 0);
    EXPECT_THROW(retained->inetTCPSend({42}), std::runtime_error);
}

TEST_F(AFInetTCPTest, MoveAssignmentStopsDestinationAndRejectsRunningSource)
{
    TCPTestPeer peer;
    const auto retained = peer.authenticate(port);
    ASSERT_NE(retained, nullptr);
    EXPECT_NO_THROW(server->start());
    EXPECT_THROW(nexilis::server::af_inet::TCPServer(std::move(*server)), std::logic_error);
    nexilis::server::af_inet::TCPServer replacement(settings, port + 1);
    replacement.start();
    EXPECT_THROW(*server = std::move(replacement), std::logic_error);
    replacement.stop();
    nexilis::server::af_inet::TCPServer unstarted(settings, port + 1);
    *server = std::move(unstarted);
    EXPECT_THROW(retained->inetTCPSend({42}), std::runtime_error);
    EXPECT_NO_THROW(unstarted.stop());
    server->start();
    nexilis::server::ClientStorage::clear();
    TCPTestPeer current;
    ASSERT_NE(current.authenticate(port + 1), nullptr);
    server->stop();
    EXPECT_NO_THROW(server->start());
    EXPECT_NO_THROW(server->stop());
}

TEST_F(AFInetTCPTest, DestructionSerializesWithConcurrentSends)
{
    TCPTestPeer peer;
    const auto retained = peer.authenticate(port);
    ASSERT_NE(retained, nullptr);
    std::promise<void> sending;
    auto started = sending.get_future();
    std::jthread sender([&](std::stop_token stop)
                        {
                            try
                            {
                                retained->inetTCPSend({42});
                            }
                            catch (const std::runtime_error&)
                            {
                            }
                            sending.set_value();
                            const nexilis::nx_data bytes(4096, 42);
                            while (!stop.stop_requested())
                            {
                                try
                                {
                                    retained->inetTCPSend(bytes);
                                }
                                catch (const std::runtime_error&)
                                {
                                }
                            } });
    started.wait();
    server.reset();
    EXPECT_THROW(retained->inetTCPSend({42}), std::runtime_error);
    sender.request_stop();
}
