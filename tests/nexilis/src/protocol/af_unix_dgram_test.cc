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

#include <nexilis/client/protocol/af_unix/dgram_client.hh>
#include <nexilis/server/protocol/af_unix/dgram_server.hh>

#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <nexilis/client/protocol/af_unix/stream_client.hh>
#include <nexilis/server/protocol/af_unix/stream_server.hh>
#include <nexilis/unix_socket.hh>
#include <system_error>

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
        nexilis::server::RoomStorage::clear();
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
        nexilis::server::RoomStorage::clear();
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

std::shared_ptr<nexilis::server::User> authenticateDatagram(int peer, const std::string& path,
                                                            const std::string& peerPath)
{
    const auto endpoint = nexilis::unix_socket::address(path);
    const std::string password = "secret";
    EXPECT_EQ(sendto(peer, password.data(), password.size(), 0,
                     reinterpret_cast<const sockaddr*>(&endpoint), sizeof(endpoint)),
              static_cast<ssize_t>(password.size()));
    timeval timeout{};
    timeout.tv_sec = 2;
    EXPECT_EQ(setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)), 0);
    char response[1024];
    // The reply follows callback installation, so no sender races its setup.
    if (recv(peer, response, sizeof(response), 0) <= 0)
    {
        ADD_FAILURE() << "Datagram authentication did not reply";
        return {};
    }
    const auto users = nexilis::server::ClientStorage::getClientsByIpAddress(peerPath);
    if (users.size() != 1)
    {
        ADD_FAILURE() << "Expected one authenticated datagram user";
        return {};
    }
    return users.front();
}

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

TEST_F(UnixSocketSecurityTest, IdleDatagramTransportRemovesUserAndLeavesRoom)
{
    settings.setDatagramIdleTimeout(std::chrono::milliseconds(500));

    const auto path = directory + "/idle-server";
    const auto peerPath = directory + "/idle-peer";
    TestFd peer(socket(AF_UNIX, SOCK_DGRAM, 0));
    ASSERT_NE(peer.fd, -1);
    nexilis::unix_socket::bindPrivate(peer.fd, peerPath);

    nexilis::server::af_unix::DgramServer server(settings, path);
    server.start();

    const auto user = authenticateDatagram(peer.fd, path, peerPath);
    ASSERT_NE(user, nullptr);
    const auto userId = user->getId();

    const auto roomId = attachRoom(user, "idle_unix_dgram_room");
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

TEST_F(UnixSocketSecurityTest, StreamDisconnectRemovesUserAndLeavesRoom)
{
    const auto path = directory + "/stream-disconnect";
    nexilis::server::af_unix::StreamServer server(settings, path);
    server.start();

    TestFd peer(socket(AF_UNIX, SOCK_STREAM, 0));
    ASSERT_NE(peer.fd, -1);
    const auto endpoint = nexilis::unix_socket::address(path);
    ASSERT_EQ(connect(peer.fd, reinterpret_cast<const sockaddr*>(&endpoint), sizeof(endpoint)), 0);
    const auto frame = nexilis::client::ClientProtocol::frame({'s', 'e', 'c', 'r', 'e', 't'});
    ASSERT_EQ(send(peer.fd, frame.data(), frame.size(), MSG_NOSIGNAL),
              static_cast<ssize_t>(frame.size()));

    timeval timeout{};
    timeout.tv_sec = 2;
    ASSERT_EQ(setsockopt(peer.fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)), 0);
    char response[1024];
    ASSERT_GT(recv(peer.fd, response, sizeof(response), 0), 0);

    auto users = nexilis::server::ClientStorage::getClientsByIpAddress("127.0.0.1");
    ASSERT_EQ(users.size(), 1);
    const auto user = users.front();
    const auto userId = user->getId();

    const auto roomId = attachRoom(user, "stream_disconnect_room");
    ASSERT_NE(roomId, 0u);
    auto room = nexilis::server::RoomStorage::getRoomById(roomId);
    ASSERT_NE(room, nullptr);
    ASSERT_TRUE(room->contains(userId));

    ::close(peer.fd);
    peer.fd = -1;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (nexilis::server::ClientStorage::contains(userId) &&
           std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_FALSE(nexilis::server::ClientStorage::contains(userId));
    EXPECT_FALSE(room->contains(userId));
}

TEST_F(UnixSocketSecurityTest, DatagramCallbackIsClosedAfterStopAndServerReplacement)
{
    const auto path = directory + "/server";
    const auto peerPath = directory + "/peer";
    TestFd peer(socket(AF_UNIX, SOCK_DGRAM, 0));
    ASSERT_NE(peer.fd, -1);
    nexilis::unix_socket::bindPrivate(peer.fd, peerPath);
    std::shared_ptr<nexilis::server::User> retained;
    {
        nexilis::server::af_unix::DgramServer server(settings, path);
        server.start();
        retained = authenticateDatagram(peer.fd, path, peerPath);
        ASSERT_NE(retained, nullptr);
        ASSERT_TRUE(retained->unixDgramSend({42}));
        char byte{};
        ASSERT_EQ(recv(peer.fd, &byte, 1, 0), 1);
        EXPECT_EQ(byte, 42);
        server.stop();
        EXPECT_THROW(retained->unixDgramSend({42}), std::runtime_error);
    }
    EXPECT_THROW(retained->unixDgramSend({42}), std::runtime_error);

    // Reusing the server path/socket must not reactivate the old callback.
    nexilis::server::ClientStorage::clear();
    nexilis::server::af_unix::DgramServer replacement(settings, path);
    replacement.start();
    const auto current = authenticateDatagram(peer.fd, path, peerPath);
    ASSERT_NE(current, nullptr);
    EXPECT_THROW(retained->unixDgramSend({42}), std::runtime_error);
    ASSERT_TRUE(current->unixDgramSend({43}));
    char byte{};
    ASSERT_EQ(recv(peer.fd, &byte, 1, 0), 1);
    EXPECT_EQ(byte, 43);
}

TEST_F(UnixSocketSecurityTest, DatagramStopAndDestructionSerializeWithConcurrentSends)
{
    const auto path = directory + "/server";
    const auto peerPath = directory + "/peer";
    TestFd peer(socket(AF_UNIX, SOCK_DGRAM, 0));
    ASSERT_NE(peer.fd, -1);
    nexilis::unix_socket::bindPrivate(peer.fd, peerPath);
    auto server = std::make_unique<nexilis::server::af_unix::DgramServer>(settings, path);
    server->start();
    const auto retained = authenticateDatagram(peer.fd, path, peerPath);
    ASSERT_NE(retained, nullptr);

    std::promise<void> sending;
    auto started = sending.get_future();
    std::atomic<bool> queueFull{false};
    std::jthread sender([&](std::stop_token stop)
                        {
                            sending.set_value();
                            while (!stop.stop_requested())
                            {
                                try
                                {
                                    retained->unixDgramSend({42});
                                }
                                catch (const std::system_error& error)
                                {
                                    if (error.code().value() == EAGAIN || error.code().value() == EWOULDBLOCK)
                                        queueFull = true;
                                }
                                catch (const std::runtime_error&)
                                {
                                    // Shutdown has disabled this callback.
                                }
                            } });
    started.wait();
    // Fill the undrained peer queue, exercising the nonblocking send path.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!queueFull.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    EXPECT_TRUE(queueFull.load());
    const auto before = std::chrono::steady_clock::now();
    server->stop();
    server.reset();
    EXPECT_LT(std::chrono::steady_clock::now() - before, std::chrono::seconds(1));
    EXPECT_THROW(retained->unixDgramSend({42}), std::runtime_error);
    sender.request_stop();
}

TEST_F(UnixSocketSecurityTest, DatagramMoveAssignmentClosesPreviousCallbacks)
{
    const auto path = directory + "/server";
    const auto peerPath = directory + "/peer";
    TestFd peer(socket(AF_UNIX, SOCK_DGRAM, 0));
    ASSERT_NE(peer.fd, -1);
    nexilis::unix_socket::bindPrivate(peer.fd, peerPath);
    nexilis::server::af_unix::DgramServer server(settings, path);
    server.start();
    const auto retained = authenticateDatagram(peer.fd, path, peerPath);
    ASSERT_NE(retained, nullptr);

    const auto replacementPath = directory + "/replacement";
    nexilis::server::af_unix::DgramServer replacement(settings, replacementPath);
    server = std::move(replacement);
    EXPECT_THROW(retained->unixDgramSend({42}), std::runtime_error);
    EXPECT_FALSE(std::filesystem::exists(path));
    // The transferred socket is usable and independent of the old callback.
    nexilis::server::ClientStorage::clear();
    server.start();
    ASSERT_NE(authenticateDatagram(peer.fd, replacementPath, peerPath), nullptr);
}

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
        const auto users = nexilis::server::ClientStorage::getClientsByIpAddress("127.0.0.1");
        ASSERT_EQ(users.size(), 1);
        retained = users.front();
        const auto start = std::chrono::steady_clock::now();
        server.stop();
        EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(1));
    }
    // The callback outlives the server, but owns closed connection state.
    EXPECT_THROW(retained->unixStreamSend({1, 2, 3}), std::runtime_error);
}
} // namespace
