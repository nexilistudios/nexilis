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

// Windows-only regression coverage for the Boost.Asio UDP client.
//
// Unlike the BSD sockets used on Linux, Windows rejects receive_from() on a
// socket that has not been bound (WSAEINVAL). UDPClient::start() therefore binds
// an ephemeral local endpoint before entering its receive loop. The tests below
// only build and run on Windows; they would fail if that bind were removed,
// because the client could never complete the handshake.
//
// Build the Windows test binary with a Windows toolchain (for example the
// MinGW-w64 cross toolchain in nexilis/cmake/mingw_toolchain.cmake) and run it
// on Windows or under Wine.

#include <gtest/gtest.h>

#if defined(_WIN32)

#include <nexilis/client/packet.hh>
#include <nexilis/logger/log.hh>
#include <nexilis/ports.hh>
#include <nexilis/protocol_manager.hh>
#include <nexilis/server/client_storage.hh>

#include <nexilis/client/protocol/nxboost/udp_client.hh>
#include <nexilis/server/protocol/nxboost/udp_server.hh>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

namespace
{

// Distinct from the port used by the cross-platform BoostUDPTest so the two
// suites can never share a listening socket.
constexpr std::uint16_t kWindowsBoostUDPTestPort = 55231;

class WindowsBoostUDPClientTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nexilis::Log::startConsoleDebugging();

        m_previousPort = nexilis::Ports::getBoostUDPPort();
        nexilis::Ports::setBoostUDPPort(kWindowsBoostUDPTestPort);

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
        server_data.setBoostUDPServerPort(kWindowsBoostUDPTestPort);
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
        nexilis::server::ClientStorage::clear();
        nexilis::Log::stopLogging();
        nexilis::Ports::setBoostUDPPort(m_previousPort);
    }

    bool waitForConnection(std::chrono::seconds timeout = std::chrono::seconds(10))
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!client->isConnected() && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return client->isConnected();
    }

    void waitForRoomCount(std::size_t expected, std::chrono::seconds timeout = std::chrono::seconds(5))
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (api->getActiveRooms().size() != expected && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    nexilis::ProtocolManager protocol_manager;
    nexilis::server::ServerConfig settings;

    std::uint16_t m_previousPort = 0;
    std::unique_ptr<nexilis::client::ClientAPI> api;
    std::shared_ptr<nexilis::server::nxboost::UDPServer> server;
    std::shared_ptr<nexilis::client::nxboost::UDPClient> client;
};

} // namespace

// Exercises the bind-before-receive fix: connecting and receiving a server reply
// is only possible if the client socket is bound on Windows.
TEST_F(WindowsBoostUDPClientTest, ConnectsAndReceivesServerReplies)
{
    client->start();
    ASSERT_TRUE(waitForConnection());

    auto payload = nexilis::client::Packet::Room::Management::create(*api, "windows_room");
    auto future = client->sendMessageAsync(payload);

    ASSERT_EQ(future.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    EXPECT_NO_THROW(future.get());

    waitForRoomCount(1);
    EXPECT_EQ(api->getActiveRooms().size(), 1);
}

// Repeated stop() calls happen during teardown and on shutdown paths; on Windows
// the underlying socket shutdown/cancel/close sequence must tolerate them.
TEST_F(WindowsBoostUDPClientTest, StopIsIdempotent)
{
    client->start();
    ASSERT_TRUE(waitForConnection());

    client->stop();
    EXPECT_NO_THROW(client->stop());
    EXPECT_FALSE(client->isConnected());
}

// A UDP server that disappears causes Windows to surface ICMP "port unreachable"
// as WSAECONNRESET on later socket operations. Every async send must still
// complete its future instead of hanging or terminating the process.
TEST_F(WindowsBoostUDPClientTest, AsyncSendsResolveAfterServerStops)
{
    client->start();
    ASSERT_TRUE(waitForConnection());

    server->stop();

    for (int i = 0; i < 3; ++i)
    {
        auto payload = nexilis::client::Packet::Room::Management::create(
                *api, "gone_" + std::to_string(i));
        auto future = client->sendMessageAsync(payload);

        ASSERT_EQ(future.wait_for(std::chrono::seconds(5)), std::future_status::ready);

        try
        {
            future.get();
        }
        catch (const std::exception&)
        {
            // A send may legitimately fail once the server is gone; the point is
            // that the future resolves rather than blocking forever.
        }
    }
}

#endif // _WIN32
