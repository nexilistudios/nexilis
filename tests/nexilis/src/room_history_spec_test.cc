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

#include <nexilis/client/client_api.hh>
#include <nexilis/client/command_parser.hh>
#include <nexilis/crypto.hh>
#include <nexilis/json.hh>
#include <nexilis/nexilis_constants.hh>
#include <nexilis/room_command_type.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/command/command_result.hh>
#include <nexilis/server/room_storage.hh>
#include <nexilis/server/server_json.hh>
#include <nexilis/server/user.hh>
#include <nexilis/util.hh>

#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace nexilis;

namespace
{

constexpr uint64_t ROOM_ID = 777;
constexpr uint64_t SENDER_ID = 100;
constexpr uint64_t JOINER_ID = 200;

/// Protocol that routes outbound messages through the User boost TCP callback.
class BoostTCPServerProtocol : public NXProtocol
{
public:
    void start() override
    {
    }
    void stop() override
    {
    }
    NXProtocol::Type getType() override
    {
        return NXProtocol::Type::BOOST_TCP_SERVER;
    }
};

class RoomHistorySpecTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        server::RoomStorage::clear();
        server::ClientStorage::clear();

        server::RoomStorage::add(server::Room(RoomData(SENDER_ID, "history_room", ROOM_ID)));
        server::RoomStorage::getRoomById(ROOM_ID)->setBroadcastHistoryEnabled(true);
        server::RoomStorage::getRoomById(ROOM_ID)->setAdmissionHandler(
                [](const server::User&)
                { return true; });
        server::RoomStorage::getRoomById(ROOM_ID)->setBroadcastHandler(
                [](uint64_t, const std::string&, bool)
                { return true; });

        auto sender = std::make_unique<server::User>(SENDER_ID, "127.0.0.1");
        server::ClientStorage::add(std::move(sender));

        auto joiner = std::make_unique<server::User>(JOINER_ID, "127.0.0.2");
        server::ClientStorage::add(std::move(joiner));
    }

    void TearDown() override
    {
        server::RoomStorage::clear();
        server::ClientStorage::clear();
    }

    server::ServerConfig settings;
    BoostTCPServerProtocol protocol;
    server::Command command{settings};

    std::shared_ptr<server::Room> room() const
    {
        return server::RoomStorage::getRoomById(ROOM_ID);
    }

    std::shared_ptr<server::User> user(uint64_t id) const
    {
        return server::ClientStorage::getClientById(id);
    }

    /// Build broadcast command bytes: room / communication / broadcast / message.
    nx_data broadcastBytes(const std::string& message, bool encrypted = false) const
    {
        nx_data bytes{static_cast<uint8_t>(CommandType::room),
                      static_cast<uint8_t>(RoomCommandType::Root::communication),
                      static_cast<uint8_t>(encrypted
                                                   ? RoomCommandType::Communication::broadcast_encrypted
                                                   : RoomCommandType::Communication::broadcast)};
        bytes.insert(bytes.end(), message.begin(), message.end());
        return bytes;
    }

    /// Build join command bytes: room / management / join / room id.
    nx_data joinBytes(uint64_t targetRoomId = ROOM_ID) const
    {
        nx_data bytes{static_cast<uint8_t>(CommandType::room),
                      static_cast<uint8_t>(RoomCommandType::Root::management),
                      static_cast<uint8_t>(RoomCommandType::Management::join)};
        auto idBytes = Util::convertToByteVector(targetRoomId);
        bytes.insert(bytes.end(), idBytes.begin(), idBytes.end());
        return bytes;
    }

    /// Collect all JSON messages the given user receives.
    std::vector<boost::json::object> receivedMessages(uint64_t clientId)
    {
        std::vector<boost::json::object> messages;
        auto client = user(clientId);
        client->setBoostTCPSend([&messages](const nx_data& data)
                                {
            nx_data jsonData = data;
            if (!jsonData.empty() && jsonData.back() == '\n')
            {
                jsonData.pop_back();
            }
            messages.emplace_back(Json::convertToJSON(jsonData)); });
        return messages;
    }
};

TEST_F(RoomHistorySpecTest, BroadcastIsStoredInRoom)
{
    room()->joinRoom(SENDER_ID);
    user(SENDER_ID)->setRoomId(ROOM_ID);

    const std::string message = "hello \xC3\xA4\xC3\xB6";
    ASSERT_EQ(command.read(broadcastBytes(message), *user(SENDER_ID), protocol, 7),
              server::CommandResult::success);

    const auto& broadcasts = room()->getBroadcasts();
    ASSERT_EQ(broadcasts.size(), 1);
    EXPECT_EQ(broadcasts[0].clientId, SENDER_ID);
    EXPECT_EQ(broadcasts[0].message, message);
}

TEST_F(RoomHistorySpecTest, BroadcastHistoryRequiresExplicitOptIn)
{
    server::Room fresh(RoomData(SENDER_ID, "fresh", ROOM_ID + 1));
    fresh.addBroadcast(SENDER_ID, "not retained");
    EXPECT_TRUE(fresh.getBroadcasts().empty());

    room()->setBroadcastHistoryEnabled(false);
    room()->joinRoom(SENDER_ID);
    user(SENDER_ID)->setRoomId(ROOM_ID);
    auto replayMessages = receivedMessages(JOINER_ID);
    EXPECT_EQ(command.read(broadcastBytes("live only"), *user(SENDER_ID), protocol, 1),
              server::CommandResult::success);
    EXPECT_TRUE(room()->getBroadcasts().empty());
    ASSERT_EQ(command.read(joinBytes(), *user(JOINER_ID), protocol, 2),
              server::CommandResult::success);
    EXPECT_EQ(std::count_if(replayMessages.begin(), replayMessages.end(),
                            [](const auto& message)
                            { return message.contains("type") &&
                                     message.at("type").as_string() == "communication"; }),
              0);

    room()->setBroadcastHistoryEnabled(true);
    room()->addBroadcast(SENDER_ID, "retained");
    ASSERT_EQ(room()->getBroadcasts().size(), 1u);
    room()->setBroadcastHistoryEnabled(false);
    EXPECT_TRUE(room()->getBroadcasts().empty());
}

TEST_F(RoomHistorySpecTest, BroadcastRequiresRoomApproval)
{
    room()->joinRoom(SENDER_ID);
    user(SENDER_ID)->setRoomId(ROOM_ID);
    command.getServerConfig().getGameplayLimits().setMaxBroadcastRate(0.0f);
    auto messages = receivedMessages(SENDER_ID);
    room()->setBroadcastHandler({});
    EXPECT_EQ(command.read(broadcastBytes("server: win"), *user(SENDER_ID), protocol, 1),
              server::CommandResult::unauthorized);
    room()->setBroadcastHandler([](uint64_t playerId, const std::string& message, bool)
                                { return playerId == SENDER_ID && message == "chat: hello"; });
    EXPECT_EQ(command.read(broadcastBytes("server: win"), *user(SENDER_ID), protocol, 2),
              server::CommandResult::unauthorized);
    EXPECT_TRUE(room()->getBroadcasts().empty());
    EXPECT_TRUE(messages.empty());
    EXPECT_EQ(command.read(broadcastBytes("chat: hello"), *user(SENDER_ID), protocol, 3),
              server::CommandResult::success);
    ASSERT_EQ(room()->getBroadcasts().size(), 1);
    ASSERT_EQ(messages.size(), 1);
    EXPECT_TRUE(messages[0].at("player_authored").as_bool());
    EXPECT_FALSE(messages[0].at("replayed").as_bool());
}

TEST_F(RoomHistorySpecTest, ServerRoomEventsStaySeparateFromPlayerBroadcasts)
{
    room()->joinRoom(SENDER_ID);
    user(SENDER_ID)->setRoomId(ROOM_ID);
    auto messages = receivedMessages(SENDER_ID);
    const boost::json::value state = boost::json::object{{"score", 10}};
    ASSERT_TRUE(server::Command::sendServerRoomEvent(ROOM_ID, "score", state, protocol));
    ASSERT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0].at("type").as_string(), "server_event");
    EXPECT_EQ(messages[0].at("event").as_string(), "score");
    EXPECT_EQ(messages[0].at("payload"), state);
    EXPECT_FALSE(messages[0].contains("player_authored"));
    EXPECT_TRUE(room()->getBroadcasts().empty());

    client::ClientConfig config;
    client::ClientAPI api(config);
    client::ClientAPI::ClientAPIData data;
    auto eventCommand = client::CommandParser::parse(messages[0]);
    ASSERT_EQ(eventCommand->execute(api, data), client::ReadResult::success);
    auto events = data.consumeServerRoomEvents();
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].room_id, ROOM_ID);
    EXPECT_EQ(events[0].event, "score");
    EXPECT_EQ(events[0].payload, state);

    EXPECT_EQ(command.read(broadcastBytes("score: 999"), *user(SENDER_ID), protocol, 2),
              server::CommandResult::success);
    ASSERT_EQ(messages.size(), 2);
    EXPECT_EQ(messages[1].at("type").as_string(), "communication");
    EXPECT_TRUE(messages[1].at("player_authored").as_bool());
    auto chatCommand = client::CommandParser::parse(messages[1]);
    data.getCurrentlyActiveRooms().emplace_back(
            RoomData(SENDER_ID, "room", ROOM_ID), std::vector<client::ClientSession>{});
    EXPECT_EQ(chatCommand->execute(api, data), client::ReadResult::success);
    EXPECT_EQ(data.getCurrentlyActiveRooms()[0].getMessages().size(), 1);
    EXPECT_FALSE(data.getCurrentlyActiveRooms()[0].getMessages()[0].wasReplayed());
    EXPECT_TRUE(data.consumeServerRoomEvents().empty());
}

TEST_F(RoomHistorySpecTest, UnmarkedBroadcastCannotEnterClientChat)
{
    client::ClientConfig config;
    client::ClientAPI api(config);
    client::ClientAPI::ClientAPIData data;
    data.getCurrentlyActiveRooms().emplace_back(
            RoomData(SENDER_ID, "room", ROOM_ID), std::vector<client::ClientSession>{});
    boost::json::object message{{"command", "room"}, {"type", "communication"}, {"action", "broadcast"}, {"room_id", ROOM_ID}, {"client_id", SENDER_ID}, {"message", "server: win"}};
    auto parsed = client::CommandParser::parse(message);
    EXPECT_NE(parsed->execute(api, data), client::ReadResult::success);
    EXPECT_TRUE(data.getCurrentlyActiveRooms()[0].getMessages().empty());
}

TEST_F(RoomHistorySpecTest, MissingReplayMarkerIsConservativelyHistorical)
{
    client::ClientConfig config;
    client::ClientAPI api(config);
    client::ClientAPI::ClientAPIData data;
    data.getCurrentlyActiveRooms().emplace_back(
            RoomData(SENDER_ID, "room", ROOM_ID), std::vector<client::ClientSession>{});
    boost::json::object message{{"command", "room"}, {"type", "communication"}, {"action", "broadcast"}, {"room_id", ROOM_ID}, {"client_id", SENDER_ID}, {"message", "old chat"}};
    message["player_authored"] = true;
    auto parsed = client::CommandParser::parse(message);
    EXPECT_EQ(parsed->execute(api, data), client::ReadResult::success);
    const auto& messages = data.getCurrentlyActiveRooms()[0].getMessages();
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_TRUE(messages[0].wasReplayed());

    message["replayed"] = "false";
    parsed = client::CommandParser::parse(message);
    EXPECT_NE(parsed->execute(api, data), client::ReadResult::success);
    EXPECT_EQ(messages.size(), 1u);
}

TEST_F(RoomHistorySpecTest, EncryptedBroadcastRetainsActionInHistory)
{
    room()->joinRoom(SENDER_ID);
    user(SENDER_ID)->setRoomId(ROOM_ID);
    room()->setBroadcastHandler([](uint64_t, const std::string&, bool encrypted)
                                { return encrypted; });
    auto liveMessages = receivedMessages(SENDER_ID);
    const auto key = crypto::deriveMessageKey("salasana", ROOM_ID);
    std::string sealed;
    ASSERT_TRUE(crypto::encrypt(key, "secret chat", sealed));
    const std::string ciphertext = crypto::toBase64(sealed);
    EXPECT_EQ(command.read(broadcastBytes(ciphertext, true), *user(SENDER_ID), protocol, 1),
              server::CommandResult::success);
    ASSERT_EQ(room()->getBroadcasts().size(), 1);
    EXPECT_TRUE(room()->getBroadcasts()[0].encrypted);
    ASSERT_EQ(liveMessages.size(), 1);
    EXPECT_EQ(liveMessages[0].at("action").as_string(), "broadcast_encrypted");
    EXPECT_TRUE(liveMessages[0].at("player_authored").as_bool());
    EXPECT_FALSE(liveMessages[0].at("replayed").as_bool());

    auto replayMessages = receivedMessages(JOINER_ID);
    EXPECT_EQ(command.read(joinBytes(), *user(JOINER_ID), protocol, 2),
              server::CommandResult::success);
    auto replay = std::find_if(replayMessages.begin(), replayMessages.end(),
                               [](const auto& message)
                               { return message.contains("action") &&
                                        message.at("action").as_string() == "broadcast_encrypted"; });
    ASSERT_NE(replay, replayMessages.end());
    EXPECT_EQ(replay->at("message").as_string(), ciphertext);
    EXPECT_TRUE(replay->at("player_authored").as_bool());
    EXPECT_TRUE(replay->at("replayed").as_bool());
}

TEST_F(RoomHistorySpecTest, BroadcastWithoutRoomFails)
{
    ASSERT_EQ(command.read(broadcastBytes("nope"), *user(SENDER_ID), protocol, 1),
              server::CommandResult::error);
    EXPECT_TRUE(room()->getBroadcasts().empty());
}

TEST_F(RoomHistorySpecTest, JoiningAnotherRoomRequiresLeavingFirst)
{
    constexpr uint64_t otherRoomId = ROOM_ID + 1;
    ASSERT_TRUE(server::RoomStorage::add(server::Room(RoomData(SENDER_ID, "other", otherRoomId))));
    auto joinOther = joinBytes();
    auto otherIdBytes = Util::convertToByteVector(otherRoomId);
    std::copy(otherIdBytes.begin(), otherIdBytes.end(), joinOther.begin() + 3);

    auto joiner = user(JOINER_ID);
    ASSERT_EQ(command.read(joinBytes(), *joiner, protocol, 1), server::CommandResult::success);
    EXPECT_EQ(joiner->getRoomId(), ROOM_ID);
    EXPECT_TRUE(room()->contains(JOINER_ID));

    EXPECT_EQ(command.read(joinOther, *joiner, protocol, 2), server::CommandResult::invalid_input);
    EXPECT_EQ(joiner->getRoomId(), ROOM_ID);
    EXPECT_TRUE(room()->contains(JOINER_ID));
    EXPECT_FALSE(server::RoomStorage::getRoomById(otherRoomId)->contains(JOINER_ID));

    nx_data joinZero = joinBytes();
    std::fill(joinZero.begin() + 3, joinZero.end(), 0);
    EXPECT_EQ(command.read(joinZero, *joiner, protocol, 4), server::CommandResult::invalid_input);

    nx_data leaveBytes{static_cast<uint8_t>(CommandType::room),
                       static_cast<uint8_t>(RoomCommandType::Root::management),
                       static_cast<uint8_t>(RoomCommandType::Management::leave)};
    EXPECT_EQ(command.read(leaveBytes, *joiner, protocol, 3), server::CommandResult::success);
    EXPECT_EQ(joiner->getRoomId(), 0);
    EXPECT_FALSE(room()->contains(JOINER_ID));
    EXPECT_FALSE(server::RoomStorage::getRoomById(otherRoomId)->contains(JOINER_ID));
}

TEST_F(RoomHistorySpecTest, RoomsArePrivateWithoutAdmissionHandler)
{
    constexpr uint64_t privateRoomId = ROOM_ID + 10;
    server::RoomStorage::add(server::Room(RoomData(SENDER_ID, "private", privateRoomId)));
    auto privateRoom = server::RoomStorage::getRoomById(privateRoomId);
    ASSERT_NE(privateRoom, nullptr);

    auto creator = user(SENDER_ID);
    auto outsider = user(JOINER_ID);
    EXPECT_TRUE(privateRoom->allowsUser(*creator));
    EXPECT_FALSE(privateRoom->allowsUser(*outsider));

    EXPECT_EQ(command.read(joinBytes(privateRoomId), *creator, protocol, 6),
              server::CommandResult::success);
    EXPECT_TRUE(privateRoom->contains(SENDER_ID));
    EXPECT_EQ(command.read(joinBytes(privateRoomId), *outsider, protocol, 7),
              server::CommandResult::invalid_input);
    EXPECT_FALSE(privateRoom->contains(JOINER_ID));

    auto visibleRooms = server::ServerJson::getRoomData(outsider.get());
    ASSERT_EQ(visibleRooms.at("room_amount").as_uint64(), 1);
    EXPECT_EQ(visibleRooms.at("rooms").as_array()[0].at("room_id").as_uint64(), ROOM_ID);

    auto visibleClients = server::ServerJson::getClientData(outsider.get());
    EXPECT_EQ(visibleClients.at("client_amount").as_uint64(), 1);
}

TEST_F(RoomHistorySpecTest, AdmissionPolicyHidesRoomAndRejectsJoin)
{
    room()->setAdmissionHandler([](const server::User& candidate)
                                { return candidate.getId() == SENDER_ID; });
    room()->joinRoom(SENDER_ID);
    user(SENDER_ID)->setRoomId(ROOM_ID);

    auto denied = user(JOINER_ID);
    EXPECT_EQ(command.read(joinBytes(), *denied, protocol, 5), server::CommandResult::invalid_input);
    EXPECT_EQ(denied->getRoomId(), 0);
    EXPECT_FALSE(room()->contains(JOINER_ID));

    auto hiddenRooms = server::ServerJson::getRoomData(denied.get());
    EXPECT_EQ(hiddenRooms.at("room_amount").as_int64(), 0);
    EXPECT_TRUE(hiddenRooms.at("rooms").as_array().empty());
    auto hiddenClients = server::ServerJson::getClientData(denied.get());
    EXPECT_EQ(hiddenClients.at("client_amount").as_uint64(), 1);

    auto allowedRooms = server::ServerJson::getRoomData(user(SENDER_ID).get());
    EXPECT_EQ(allowedRooms.at("room_amount").as_uint64(), 1);
}

TEST_F(RoomHistorySpecTest, JoinSendsStoredBroadcasts)
{
    room()->joinRoom(SENDER_ID);
    user(SENDER_ID)->setRoomId(ROOM_ID);
    room()->addBroadcast(SENDER_ID, "first message");
    room()->addBroadcast(SENDER_ID, "second message");

    auto messages = receivedMessages(JOINER_ID);
    ASSERT_EQ(command.read(joinBytes(), *user(JOINER_ID), protocol, 1),
              server::CommandResult::success);

    std::vector<std::string> broadcastPayloads;
    client::ClientConfig config;
    client::ClientAPI api(config);
    client::ClientAPI::ClientAPIData data;
    data.getCurrentlyActiveRooms().emplace_back(
            RoomData(SENDER_ID, "room", ROOM_ID), std::vector<client::ClientSession>{});
    for (const auto& msg : messages)
    {
        if (msg.contains("type") && msg.at("type").as_string() == "communication" &&
            msg.contains("action") && msg.at("action").as_string() == "broadcast")
        {
            EXPECT_TRUE(msg.at("replayed").as_bool());
            broadcastPayloads.emplace_back(msg.at("message").as_string().c_str());
            auto parsed = client::CommandParser::parse(msg);
            EXPECT_EQ(parsed->execute(api, data), client::ReadResult::success);
        }
    }

    ASSERT_EQ(broadcastPayloads.size(), 2);
    EXPECT_EQ(broadcastPayloads[0], "first message");
    EXPECT_EQ(broadcastPayloads[1], "second message");
    const auto& received = data.getCurrentlyActiveRooms()[0].getMessages();
    ASSERT_EQ(received.size(), 2u);
    EXPECT_TRUE(received[0].wasReplayed());
    EXPECT_TRUE(received[1].wasReplayed());
}

TEST_F(RoomHistorySpecTest, JoinHistoryKeepsOriginalSender)
{
    room()->joinRoom(SENDER_ID);
    user(SENDER_ID)->setRoomId(ROOM_ID);
    room()->addBroadcast(SENDER_ID, "sender's message");

    auto messages = receivedMessages(JOINER_ID);
    ASSERT_EQ(command.read(joinBytes(), *user(JOINER_ID), protocol, 1),
              server::CommandResult::success);

    for (const auto& msg : messages)
    {
        if (msg.contains("type") && msg.at("type").as_string() == "communication" &&
            msg.contains("action") && msg.at("action").as_string() == "broadcast")
        {
            EXPECT_EQ(Util::toUint64(msg.at("client_id")), SENDER_ID);
            EXPECT_EQ(Util::toUint64(msg.at("room_id")), ROOM_ID);
        }
    }
}

TEST_F(RoomHistorySpecTest, BroadcastRejectsOverlongMessage)
{
    room()->joinRoom(SENDER_ID);
    user(SENDER_ID)->setRoomId(ROOM_ID);
    command.getServerConfig().getGameplayLimits().setMaxBroadcastLength(8);

    EXPECT_EQ(command.read(broadcastBytes("123456789"), *user(SENDER_ID), protocol, 1),
              server::CommandResult::invalid_input);
    EXPECT_TRUE(room()->getBroadcasts().empty());

    // A message at the configured limit is still accepted.
    ASSERT_EQ(command.read(broadcastBytes("12345678"), *user(SENDER_ID), protocol, 2),
              server::CommandResult::success);
    ASSERT_EQ(room()->getBroadcasts().size(), 1);
    EXPECT_EQ(room()->getBroadcasts().front().message, "12345678");
}

TEST_F(RoomHistorySpecTest, BroadcastRateLimitDropsFastFollowUp)
{
    room()->joinRoom(SENDER_ID);
    user(SENDER_ID)->setRoomId(ROOM_ID);
    command.getServerConfig().getGameplayLimits().setMaxBroadcastRate(1.0f);

    ASSERT_EQ(command.read(broadcastBytes("first"), *user(SENDER_ID), protocol, 1),
              server::CommandResult::success);
    EXPECT_EQ(command.read(broadcastBytes("second"), *user(SENDER_ID), protocol, 2),
              server::CommandResult::invalid_input);

    const auto broadcasts = room()->getBroadcasts();
    ASSERT_EQ(broadcasts.size(), 1);
    EXPECT_EQ(broadcasts.front().message, "first");
}

TEST_F(RoomHistorySpecTest, BroadcastRateLimitIsPerClient)
{
    room()->joinRoom(SENDER_ID);
    room()->joinRoom(JOINER_ID);
    user(SENDER_ID)->setRoomId(ROOM_ID);
    user(JOINER_ID)->setRoomId(ROOM_ID);
    command.getServerConfig().getGameplayLimits().setMaxBroadcastRate(1.0f);

    ASSERT_EQ(command.read(broadcastBytes("sender"), *user(SENDER_ID), protocol, 1),
              server::CommandResult::success);
    ASSERT_EQ(command.read(broadcastBytes("joiner"), *user(JOINER_ID), protocol, 1),
              server::CommandResult::success);
    EXPECT_EQ(command.read(broadcastBytes("sender again"), *user(SENDER_ID), protocol, 2),
              server::CommandResult::invalid_input);

    const auto broadcasts = room()->getBroadcasts();
    ASSERT_EQ(broadcasts.size(), 2);
    EXPECT_EQ(broadcasts.front().message, "sender");
    EXPECT_EQ(broadcasts.back().message, "joiner");
}

TEST_F(RoomHistorySpecTest, BroadcastRateLimitAllowsMessageAfterInterval)
{
    room()->joinRoom(SENDER_ID);
    user(SENDER_ID)->setRoomId(ROOM_ID);
    command.getServerConfig().getGameplayLimits().setMaxBroadcastRate(10.0f);

    ASSERT_EQ(command.read(broadcastBytes("first"), *user(SENDER_ID), protocol, 1),
              server::CommandResult::success);
    std::this_thread::sleep_for(std::chrono::milliseconds(110));
    ASSERT_EQ(command.read(broadcastBytes("second"), *user(SENDER_ID), protocol, 2),
              server::CommandResult::success);

    const auto broadcasts = room()->getBroadcasts();
    ASSERT_EQ(broadcasts.size(), 2);
    EXPECT_EQ(broadcasts.front().message, "first");
    EXPECT_EQ(broadcasts.back().message, "second");
}

TEST_F(RoomHistorySpecTest, BroadcastHistoryIsBounded)
{
    const uint64_t total = NEXILIS_MAX_BROADCAST_HISTORY + 5;
    for (uint64_t i = 0; i < total; ++i)
        room()->addBroadcast(SENDER_ID, std::to_string(i));

    const auto broadcasts = room()->getBroadcasts();
    ASSERT_EQ(broadcasts.size(), NEXILIS_MAX_BROADCAST_HISTORY);
    EXPECT_EQ(broadcasts.front().message, std::to_string(total - NEXILIS_MAX_BROADCAST_HISTORY));
    EXPECT_EQ(broadcasts.back().message, std::to_string(total - 1));
}

TEST_F(RoomHistorySpecTest, JoinReplaysOnlyBoundedHistory)
{
    const uint64_t total = NEXILIS_MAX_BROADCAST_HISTORY + 5;
    for (uint64_t i = 0; i < total; ++i)
        room()->addBroadcast(SENDER_ID, std::to_string(i));

    auto messages = receivedMessages(JOINER_ID);
    ASSERT_EQ(command.read(joinBytes(), *user(JOINER_ID), protocol, 1),
              server::CommandResult::success);

    std::vector<std::string> broadcastPayloads;
    for (const auto& msg : messages)
    {
        if (msg.contains("type") && msg.at("type").as_string() == "communication" &&
            msg.contains("action") && msg.at("action").as_string() == "broadcast")
        {
            broadcastPayloads.emplace_back(msg.at("message").as_string().c_str());
        }
    }

    ASSERT_EQ(broadcastPayloads.size(), NEXILIS_MAX_BROADCAST_HISTORY);
    EXPECT_EQ(broadcastPayloads.front(),
              std::to_string(total - NEXILIS_MAX_BROADCAST_HISTORY));
    EXPECT_EQ(broadcastPayloads.back(), std::to_string(total - 1));
}

} // namespace
