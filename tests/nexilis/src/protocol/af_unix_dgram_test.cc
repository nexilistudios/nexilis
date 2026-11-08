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

#include <nexilis/client/protocol/af_unix/dgram_client.hh>
#include <nexilis/server/protocol/af_unix/dgram_server.hh>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <nexilis/client/protocol/af_unix/stream_client.hh>
#include <nexilis/server/protocol/af_unix/stream_server.hh>
#include <nexilis/unix_socket.hh>

class AFUnixDgramTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nexilis::Log::startConsoleDebugging();

        std::filesystem::create_directories("/tmp/nexilis");

        settings.setMode(nexilis::server::AuthenticationMode::password_protected);
        settings.setPassphrase("salasana");
        settings.setRootPassword("root");

        server = std::make_shared<nexilis::server::af_unix::DgramServer>(
                protocol_manager.createProtocol<nexilis::server::af_unix::DgramServer>(
                        settings, "/tmp/nexilis/dgram"));
        server->start();

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        nexilis::client::ClientConfig server_data;
        server_data.setPassword("salasana");
        server_data.setUnixDgramServerPath("/tmp/nexilis/dgram");
        server_data.setMode(nexilis::server::AuthenticationMode::password_protected);

        api = std::make_unique<nexilis::client::ClientAPI>(server_data);
        client = std::make_shared<nexilis::client::af_unix::DgramClient>(
                protocol_manager.createProtocol<nexilis::client::af_unix::DgramClient>(*api));
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
    }

    nexilis::ProtocolManager protocol_manager;
    nexilis::server::ServerConfig settings;

    std::unique_ptr<nexilis::client::ClientAPI> api;

    std::shared_ptr<nexilis::server::af_unix::DgramServer> server;
    std::shared_ptr<nexilis::client::af_unix::DgramClient> client;
};

TEST_F(AFUnixDgramTest, SendMessageAsyncReturnsReadyFuture)
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

TEST_F(AFUnixDgramTest, SendMessageAsyncReturnsReadyFutureOnStoppedClient)
{
    // Don't start the client.
    auto payload = nexilis::client::Packet::Room::Management::create(*api, "test");
    auto future = client->sendMessageAsync(payload);

    auto status = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(status, std::future_status::ready);

    // Should throw since the client is stopped.
    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST_F(AFUnixDgramTest, SendMessageAsyncReturnsReadyFutureOnClosedSocket)
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

TEST_F(AFUnixDgramTest, SendMessageAsyncDeliversMessage)
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

namespace
{
class UnixSocketSecurityTest : public ::testing::Test
{
protected:
    std::string directory;
    nexilis::server::ServerConfig settings;
    void SetUp() override
    {
        char pattern[] = "/tmp/nexilis-unix-XXXXXX";
        const auto* created = mkdtemp(pattern);
        ASSERT_NE(created, nullptr);
        directory = created;
        settings.setMode(nexilis::server::AuthenticationMode::password_protected);
        settings.setPassphrase("secret");
        nexilis::server::ClientStorage::clear();
    }
    void TearDown() override
    {
        nexilis::server::ClientStorage::clear();
        if (!directory.empty())
            std::filesystem::remove_all(directory);
    }
};

struct TestFd
{
    int fd;
    explicit TestFd(int descriptor) : fd(descriptor)
    {
    }
    ~TestFd()
    {
        if (fd != -1)
            ::close(fd);
    }
    TestFd(const TestFd&) = delete;
    TestFd& operator=(const TestFd&) = delete;
};

TEST_F(UnixSocketSecurityTest, AddressRejectsInvalidPathsWithoutTruncating)
{
    sockaddr_un endpoint{};
    const std::string maximum(sizeof(endpoint.sun_path) - 1, 'a');
    EXPECT_EQ(std::string(nexilis::unix_socket::address(maximum).sun_path), maximum);
    EXPECT_THROW(nexilis::unix_socket::address(""), std::invalid_argument);
    EXPECT_THROW(nexilis::unix_socket::address(maximum + 'a'), std::invalid_argument);
    EXPECT_THROW(nexilis::unix_socket::address(std::string("a\0b", 3)), std::invalid_argument);
    const auto tooLong = maximum + 'a';
    EXPECT_THROW((nexilis::server::af_unix::StreamServer(settings, tooLong)), std::invalid_argument);
    EXPECT_THROW((nexilis::server::af_unix::DgramServer(settings, tooLong)), std::invalid_argument);
    nexilis::client::ClientConfig config;
    config.setUnixStreamServerPath(tooLong);
    nexilis::client::ClientAPI api(config);
    EXPECT_THROW((nexilis::client::af_unix::StreamClient(api)), std::invalid_argument);
}

TEST_F(UnixSocketSecurityTest, GeneratedDatagramReplyPathIsAlsoValidated)
{
    sockaddr_un endpoint{};
    nexilis::client::ClientConfig config;
    config.setUnixDgramServerPath(std::string(sizeof(endpoint.sun_path) - 1, 'a'));
    nexilis::client::ClientAPI api(config);
    nexilis::client::af_unix::DgramClient client(api);
    EXPECT_THROW(client.start(), std::invalid_argument);
}

TEST_F(UnixSocketSecurityTest, BindFailuresPreserveExistingFiles)
{
    const auto path = directory + "/occupied";
    {
        std::ofstream file(path);
        file << "keep";
    }
    EXPECT_THROW((nexilis::server::af_unix::StreamServer(settings, path)), std::system_error);
    EXPECT_THROW((nexilis::server::af_unix::DgramServer(settings, path)), std::system_error);
    std::ifstream file(path);
    std::string contents;
    file >> contents;
    EXPECT_EQ(contents, "keep");
}

TEST_F(UnixSocketSecurityTest, ServerSocketsHaveOwnerOnlyPermissions)
{
    const auto streamPath = directory + "/stream";
    const auto dgramPath = directory + "/datagram";
    {
        nexilis::server::af_unix::StreamServer stream(settings, streamPath);
        nexilis::server::af_unix::DgramServer dgram(settings, dgramPath);
        for (const auto& path : {streamPath, dgramPath})
        {
            struct stat metadata{};
            ASSERT_EQ(stat(path.c_str(), &metadata), 0);
            EXPECT_EQ(metadata.st_mode & 0777, 0600);
        }
        // A second server cannot unlink and replace the first one's socket.
        EXPECT_THROW((nexilis::server::af_unix::StreamServer(settings, streamPath)), std::system_error);
        EXPECT_TRUE(std::filesystem::exists(streamPath));
    }
    EXPECT_FALSE(std::filesystem::exists(streamPath));
    EXPECT_FALSE(std::filesystem::exists(dgramPath));
}

TEST_F(UnixSocketSecurityTest, PeerCredentialsAcceptSameUserAndRejectInvalidDescriptor)
{
    int pair[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, pair), 0);
    TestFd first(pair[0]);
    TestFd second(pair[1]);
    EXPECT_TRUE(nexilis::unix_socket::sameUser(first.fd));
    EXPECT_TRUE(nexilis::unix_socket::sameUser(second.fd));
    EXPECT_FALSE(nexilis::unix_socket::sameUser(-1));
}

TEST_F(UnixSocketSecurityTest, DatagramCredentialsComeFromKernel)
{
    int pair[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_DGRAM, 0, pair), 0);
    TestFd sender(pair[0]);
    TestFd receiver(pair[1]);
    const int enabled = 1;
    ASSERT_EQ(setsockopt(receiver.fd, SOL_SOCKET, SO_PASSCRED, &enabled, sizeof(enabled)), 0);
    ASSERT_EQ(send(sender.fd, "x", 1, 0), 1);
    char byte{};
    iovec vector{&byte, 1};
    alignas(cmsghdr) char control[CMSG_SPACE(sizeof(ucred))]{};
    msghdr message{};
    message.msg_iov = &vector;
    message.msg_iovlen = 1;
    message.msg_control = control;
    message.msg_controllen = sizeof(control);
    ASSERT_EQ(recvmsg(receiver.fd, &message, 0), 1);
    EXPECT_TRUE(nexilis::unix_socket::sameUser(message));
    message.msg_flags = MSG_CTRUNC;
    EXPECT_FALSE(nexilis::unix_socket::sameUser(message));
    msghdr missing{};
    EXPECT_FALSE(nexilis::unix_socket::sameUser(missing));
}

TEST_F(UnixSocketSecurityTest, StreamStopWakesBlockedAccept)
{
    nexilis::server::af_unix::StreamServer server(settings, directory + "/stream");
    server.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const auto start = std::chrono::steady_clock::now();
    server.stop();
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(1));
}

TEST_F(UnixSocketSecurityTest, StreamStopWakesReadAndInvalidatesStoredCallback)
{
    std::shared_ptr<nexilis::server::User> retained;
    {
        const auto path = directory + "/stream";
        nexilis::server::af_unix::StreamServer server(settings, path);
        server.start();
        TestFd peer(socket(AF_UNIX, SOCK_STREAM, 0));
        ASSERT_NE(peer.fd, -1);
        const auto endpoint = nexilis::unix_socket::address(path);
        ASSERT_EQ(connect(peer.fd, reinterpret_cast<const sockaddr*>(&endpoint), sizeof(endpoint)), 0);
        const auto frame = nexilis::client::ClientProtocol::frame({'s', 'e', 'c', 'r', 'e', 't'});
        ASSERT_EQ(send(peer.fd, frame.data(), frame.size(), MSG_NOSIGNAL), static_cast<ssize_t>(frame.size()));
        timeval timeout{};
        timeout.tv_sec = 2;
        ASSERT_EQ(setsockopt(peer.fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)), 0);
        char response[1024];
        ASSERT_GT(recv(peer.fd, response, sizeof(response), 0), 0);
        const auto start = std::chrono::steady_clock::now();
        server.stop();
        EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(1));
        const auto users = nexilis::server::ClientStorage::getClientsByIpAddress("127.0.0.1");
        ASSERT_FALSE(users.empty());
        retained = users[0];
    }
    // The callback outlives the server, but owns closed connection state.
    EXPECT_THROW(retained->unixStreamSend({1, 2, 3}), std::runtime_error);
}
} // namespace
