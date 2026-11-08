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

#include <atomic>
#include <nexilis/client/client_api.hh>
#include <nexilis/server/room_storage.hh>
#include <nexilis/util.hh>
#include <thread>

#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>

#include <string>

using namespace nexilis;

namespace
{

constexpr uint64_t ROOM_ID = 9001;
constexpr uint64_t CLIENT_A = 100;
constexpr uint64_t CLIENT_B = 200;
constexpr uint64_t CLIENT_C = 300;

/// Build a "getting"/"info_rooms" JSON object with a single room.
/// The room contains one client per entry in `clients`.
boost::json::object infoRoomsMessage(const std::vector<uint64_t>& clients)
{
    boost::json::array clientArray;
    for (auto id : clients)
    {
        boost::json::object clientObj;
        clientObj["id"] = id;
        clientObj["name"] = "player_" + std::to_string(id);
        clientObj["x"] = 0.0;
        clientObj["y"] = 0.0;
        clientObj["width"] = 1.0;
        clientObj["height"] = 1.0;
        clientArray.emplace_back(std::move(clientObj));
    }

    boost::json::object roomObj;
    roomObj["name"] = "test_room";
    roomObj["max_size"] = 10;
    roomObj["room_id"] = ROOM_ID;
    roomObj["creator_id"] = CLIENT_A;
    roomObj["context"] = 1; // RoomData::Context::_3D
    if (!clientArray.empty())
        roomObj["clients"] = std::move(clientArray);

    boost::json::object json;
    json["command"] = "getting";
    json["type"] = "info_rooms";
    json["callback"] = 0;
    json["rooms"] = boost::json::array{std::move(roomObj)};
    return json;
}

/// Build a "room"/"management" JSON object.
boost::json::object roomManagementMessage(const std::string& action, uint64_t clientId)
{
    boost::json::object json;
    json["command"] = "room";
    json["type"] = "management";
    json["callback"] = 0;
    json["action"] = action;
    json["room_id"] = ROOM_ID;
    json["client_id"] = clientId;
    return json;
}

/// Build a "room"/"player_3D" JSON object.
boost::json::object roomPlayer3DMessage(const std::string& action, uint64_t clientId,
                                        float x, float y, float z)
{
    boost::json::object json;
    json["command"] = "room";
    json["type"] = "player_3D";
    json["callback"] = 0;
    json["action"] = action;
    json["room_id"] = ROOM_ID;
    json["client_id"] = clientId;
    json["x"] = static_cast<double>(x);
    json["y"] = static_cast<double>(y);
    json["z"] = static_cast<double>(z);
    return json;
}

class RoomSpecTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        api = std::make_unique<client::ClientAPI>(config);
        api->setClientId(CLIENT_B);
    }

    client::ClientConfig config;
    std::unique_ptr<client::ClientAPI> api;

    /// Deliver a JSON command message to the client as raw wire bytes.
    client::ReadResult readMessage(const boost::json::object& json)
    {
        auto serialized = boost::json::serialize(json);
        nx_data bytes(serialized.begin(), serialized.end());
        bytes.emplace_back('\n');
        return api->readMessage(bytes);
    }

    /// The number of ClientSessions with the given id in the client's room.
    size_t sessionCount(uint64_t clientId) const
    {
        size_t count = 0;
        for (const auto& room : api->getActiveRooms())
        {
            if (room.getId() != ROOM_ID)
                continue;
            for (const auto& session : room.getClients())
            {
                if (session.getId() == clientId)
                    ++count;
            }
        }
        return count;
    }
};

TEST_F(RoomSpecTest, OwnedLookupsSurviveReplacementAndMirrorUsernameUpdates)
{
    ASSERT_EQ(readMessage(infoRoomsMessage({CLIENT_A})), client::ReadResult::success);
    auto room = api->getRoom(ROOM_ID);
    auto session = api->getClientFromRoom(CLIENT_A);
    ASSERT_NE(room, nullptr);
    ASSERT_NE(session, nullptr);
    boost::json::object update{{"command", "setting"}, {"type", "username"}, {"client_id", CLIENT_A}, {"username", "updated"}, {"callback", 0}};
    ASSERT_EQ(readMessage(update), client::ReadResult::success);
    EXPECT_EQ(api->getClientUsername(CLIENT_A), "updated");
    EXPECT_NE(session->getUsername(), "updated");
    ASSERT_EQ(readMessage(infoRoomsMessage({CLIENT_B})), client::ReadResult::success);
    EXPECT_EQ(session->getId(), CLIENT_A);
    ASSERT_EQ(room->getClients().size(), 1);
    EXPECT_EQ(room->getClients()[0].getId(), CLIENT_A);
}

TEST(ClientRoomLifetimeTest, CommunicationRetainsSenderSnapshot)
{
    auto sender = std::make_unique<client::ClientSession>(42, nullptr);
    sender->setUsername("sender");
    client::Room::Communication message("hello", sender.get());
    sender.reset();
    ASSERT_NE(message.getClient(), nullptr);
    EXPECT_EQ(message.getClient()->getId(), 42);
    EXPECT_EQ(message.getClient()->getUsername(), "sender");
}

TEST_F(RoomSpecTest, InfoRoomsCreatesSessions)
{
    ASSERT_EQ(readMessage(infoRoomsMessage({CLIENT_A})), client::ReadResult::success);

    ASSERT_EQ(api->getActiveRooms().size(), 1);
    EXPECT_EQ(sessionCount(CLIENT_A), 1);
}

TEST_F(RoomSpecTest, SetUsernameBeforeJoiningRoomSucceeds)
{
    // A client sets its username before joining any room. There is no local
    // session to mirror onto yet, but the command must still be acknowledged
    // as a success (the server stores it and delivers it with the join reply).
    boost::json::object json;
    json["command"] = "setting";
    json["type"] = "username";
    json["callback"] = 0;
    json["client_id"] = static_cast<uint64_t>(CLIENT_B);
    json["username"] = "Bob";

    EXPECT_EQ(readMessage(json), client::ReadResult::success);
}

TEST_F(RoomSpecTest, JoinAfterInfoRoomsDoesNotDuplicateSession)
{
    // The second client starts after the first is already in the room:
    // the info_rooms poll already reports the first client.
    ASSERT_EQ(readMessage(infoRoomsMessage({CLIENT_A})), client::ReadResult::success);
    EXPECT_EQ(sessionCount(CLIENT_A), 1);

    // The join flow then re-sends the existing clients to the joiner.
    ASSERT_EQ(readMessage(roomManagementMessage("join", CLIENT_A)), client::ReadResult::success);

    // The session must not be duplicated.
    EXPECT_EQ(sessionCount(CLIENT_A), 1);

    // The position sent during the join flow updates the existing session.
    ASSERT_EQ(readMessage(roomPlayer3DMessage("position", CLIENT_A, 100.0f, 200.0f, 300.0f)),
              client::ReadResult::success);

    auto players = api->getRemotePlayersSnapshot(ROOM_ID, CLIENT_B);
    ASSERT_EQ(players.size(), 1);
    EXPECT_EQ(players[0].id, CLIENT_A);
    EXPECT_FLOAT_EQ(players[0].x, 100.0f);
    EXPECT_FLOAT_EQ(players[0].y, 200.0f);
    EXPECT_FLOAT_EQ(players[0].z, 300.0f);

    auto facing = roomPlayer3DMessage("position", CLIENT_A, 100.0f, 200.0f, 300.0f);
    facing["yaw"] = 1.25;
    ASSERT_EQ(readMessage(facing), client::ReadResult::success);
    players = api->getRemotePlayersSnapshot(ROOM_ID, CLIENT_B);
    ASSERT_EQ(players.size(), 1);
    EXPECT_FLOAT_EQ(players[0].yaw, 1.25f);

    auto movement = roomPlayer3DMessage("movement", CLIENT_A, 110.0f, 200.0f, 300.0f);
    movement["yaw"] = -0.75;
    ASSERT_EQ(readMessage(movement), client::ReadResult::success);
    players = api->getRemotePlayersSnapshot(ROOM_ID, CLIENT_B);
    ASSERT_EQ(players.size(), 1);
    EXPECT_FLOAT_EQ(players[0].x, 110.0f);
    EXPECT_FLOAT_EQ(players[0].yaw, -0.75f);
}

TEST_F(RoomSpecTest, JoinBroadcastCreatesOwnSession)
{
    ASSERT_EQ(readMessage(infoRoomsMessage({})), client::ReadResult::success);
    EXPECT_FALSE(api->clientInRoom());

    ASSERT_EQ(readMessage(roomManagementMessage("join", CLIENT_B)), client::ReadResult::success);
    EXPECT_TRUE(api->clientInRoom());

    EXPECT_EQ(sessionCount(CLIENT_B), 1);
    EXPECT_TRUE(api->getRemotePlayersSnapshot(ROOM_ID, CLIENT_B).empty());
}

TEST_F(RoomSpecTest, NewJoinStillAddsClient)
{
    ASSERT_EQ(readMessage(infoRoomsMessage({CLIENT_A})), client::ReadResult::success);

    // A brand new client that was not part of the info_rooms poll still gets added.
    ASSERT_EQ(readMessage(roomManagementMessage("join", CLIENT_C)), client::ReadResult::success);

    EXPECT_EQ(sessionCount(CLIENT_A), 1);
    EXPECT_EQ(sessionCount(CLIENT_C), 1);

    auto players = api->getRemotePlayersSnapshot(ROOM_ID, CLIENT_B);
    ASSERT_EQ(players.size(), 2);
}

TEST_F(RoomSpecTest, LeaveRemovesClientSessions)
{
    ASSERT_EQ(readMessage(infoRoomsMessage({CLIENT_A})), client::ReadResult::success);
    ASSERT_EQ(readMessage(roomManagementMessage("join", CLIENT_B)), client::ReadResult::success);

    ASSERT_EQ(readMessage(roomManagementMessage("leave", CLIENT_A)), client::ReadResult::success);

    EXPECT_EQ(sessionCount(CLIENT_A), 0);
    EXPECT_EQ(sessionCount(CLIENT_B), 1);

    auto players = api->getRemotePlayersSnapshot(ROOM_ID, CLIENT_B);
    EXPECT_TRUE(players.empty());
}

} // namespace

namespace
{
class RoomStorageSpecTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        server::RoomStorage::setCreateHandler({});
        server::RoomStorage::clear();
    }
    void TearDown() override
    {
        server::RoomStorage::setCreateHandler({});
        server::RoomStorage::clear();
    }
};

TEST_F(RoomStorageSpecTest, HandlesAndSnapshotsSurviveRegistryChanges)
{
    ASSERT_TRUE(server::RoomStorage::add(server::Room(RoomData(1, "retained", 1))));
    auto held = server::RoomStorage::getRoomById(1);
    ASSERT_NE(held, nullptr);
    auto snapshot = server::RoomStorage::getAllRooms();
    std::weak_ptr<server::Room> lifetime = held;
    for (uint64_t id = 2; id < 100; ++id)
        ASSERT_TRUE(server::RoomStorage::add(server::Room(RoomData(1, "growth", id))));
    ASSERT_TRUE(server::RoomStorage::remove(1));
    EXPECT_FALSE(server::RoomStorage::contains(1));
    EXPECT_EQ(server::RoomStorage::getRoomById(1), nullptr);
    EXPECT_FALSE(server::RoomStorage::remove(1));
    server::RoomStorage::clear();
    EXPECT_EQ(held->getName(), "retained");
    ASSERT_EQ(snapshot.size(), 1);
    EXPECT_EQ(snapshot[0], held);
    held.reset();
    EXPECT_FALSE(lifetime.expired());
    snapshot.clear();
    EXPECT_TRUE(lifetime.expired());
}

TEST_F(RoomStorageSpecTest, CreationCallbackCanReenterAndRemoveRoom)
{
    bool called = false;
    server::RoomStorage::setCreateHandler([&](server::Room& room)
                                          {
        called = true;
        EXPECT_EQ(server::RoomStorage::getRoomCount(), 1);
        EXPECT_EQ(server::RoomStorage::getRoomById(room.getId()).get(), &room);
        EXPECT_TRUE(server::RoomStorage::remove(room.getId()));
        EXPECT_EQ(room.getName(), "callback"); });
    EXPECT_TRUE(server::RoomStorage::add(server::Room(RoomData(1, "callback", 1))));
    EXPECT_TRUE(called);
    EXPECT_EQ(server::RoomStorage::getRoomCount(), 0);
}

TEST_F(RoomStorageSpecTest, DuplicateIdsAreRejected)
{
    ASSERT_TRUE(server::RoomStorage::add(server::Room(RoomData(1, "original", 1))));
    EXPECT_FALSE(server::RoomStorage::add(server::Room(RoomData(2, "duplicate", 1))));
    EXPECT_EQ(server::RoomStorage::getRoomById(1)->getName(), "original");
    EXPECT_EQ(server::RoomStorage::getRoomCount(), 1);
}

TEST_F(RoomStorageSpecTest, ConcurrentInsertionsEnforceBothCaps)
{
    std::atomic<bool> start{false};
    std::atomic<unsigned> accepted{0};
    std::vector<std::thread> workers;
    for (uint64_t id = 1; id <= 8; ++id)
        workers.emplace_back([&, id]
                             {
            while (!start.load())
                std::this_thread::yield();
            if (server::RoomStorage::add(server::Room(RoomData(1, "capped", id)), 3, 2))
                ++accepted; });
    start.store(true);
    for (auto& worker : workers)
        worker.join();
    EXPECT_EQ(accepted.load(), 2);
    EXPECT_EQ(server::RoomStorage::getRoomCountByCreator(1), 2);
    EXPECT_TRUE(server::RoomStorage::add(server::Room(RoomData(2, "other", 10)), 3, 2));
    EXPECT_FALSE(server::RoomStorage::add(server::Room(RoomData(3, "full", 11)), 3, 2));
}

TEST_F(RoomStorageSpecTest, ConcurrentMutationAndEnumerationKeepHandlesAlive)
{
    std::vector<std::thread> workers;
    for (uint64_t worker = 0; worker < 4; ++worker)
        workers.emplace_back([worker]
                             {
            for (uint64_t index = 1; index <= 100; ++index)
            {
                const auto id = worker * 100 + index;
                EXPECT_TRUE(server::RoomStorage::add(server::Room(RoomData(worker, "stress", id))));
                auto held = server::RoomStorage::getRoomById(id);
                EXPECT_NE(held, nullptr);
                auto snapshot = server::RoomStorage::getAllRooms();
                EXPECT_TRUE(server::RoomStorage::remove(id));
                if (held)
                    EXPECT_EQ(held->getId(), id);
                for (const auto& room : snapshot)
                    EXPECT_EQ(room->getName(), "stress");
            } });
    for (auto& worker : workers)
        worker.join();
    EXPECT_EQ(server::RoomStorage::getRoomCount(), 0);
}
} // namespace

TEST(RoomResourceLimitTest, ConcurrentCreationCannotExceedCombinedObjectCap)
{
    server::Room room(RoomData(1, "resources", 1));
    std::atomic<unsigned> accepted{0};
    std::vector<std::thread> workers;
    for (uint64_t id = 1; id <= 16; ++id)
        workers.emplace_back([&, id]
                             {
            if (id % 2 == 0 ? room.tryAddObject(Object2D(id), 2)
                            : room.tryAddObject(Object3D(id), 2))
                ++accepted; });
    for (auto& worker : workers)
        worker.join();
    EXPECT_EQ(accepted.load(), 2);
    EXPECT_EQ(room.getObjects2D().size() + room.getObjects3D().size(), 2);
    EXPECT_FALSE(room.tryAddObject(Object2D(99), 0));
}
