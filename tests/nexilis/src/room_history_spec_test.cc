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

#include <nexilis/json.hh>
#include <nexilis/room_command_type.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/command/command_result.hh>
#include <nexilis/server/room_storage.hh>
#include <nexilis/server/user.hh>
#include <nexilis/util.hh>

#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>

#include <memory>
#include <string>
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

    server::Room* room() const
    {
        return server::RoomStorage::getRoomById(ROOM_ID);
    }

    server::User* user(uint64_t id) const
    {
        return server::ClientStorage::getClientById(id);
    }

    /// Build broadcast command bytes: room / communication / broadcast / message.
    nx_data broadcastBytes(const std::string& message) const
    {
        nx_data bytes{static_cast<uint8_t>(CommandType::room),
                      static_cast<uint8_t>(RoomCommandType::Root::communication),
                      static_cast<uint8_t>(RoomCommandType::Communication::broadcast)};
        bytes.insert(bytes.end(), message.begin(), message.end());
        return bytes;
    }

    /// Build join command bytes: room / management / join / room id.
    nx_data joinBytes() const
    {
        nx_data bytes{static_cast<uint8_t>(CommandType::room),
                      static_cast<uint8_t>(RoomCommandType::Root::management),
                      static_cast<uint8_t>(RoomCommandType::Management::join)};
        auto idBytes = Util::convertToByteVector(ROOM_ID);
        bytes.insert(bytes.end(), idBytes.begin(), idBytes.end());
        return bytes;
    }

    /// Collect all JSON messages the given user receives.
    std::vector<boost::json::object> receivedMessages(uint64_t clientId)
    {
        std::vector<boost::json::object> messages;
        auto* client = user(clientId);
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

TEST_F(RoomHistorySpecTest, BroadcastWithoutRoomFails)
{
    ASSERT_EQ(command.read(broadcastBytes("nope"), *user(SENDER_ID), protocol, 1),
              server::CommandResult::error);
    EXPECT_TRUE(room()->getBroadcasts().empty());
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
    for (const auto& msg : messages)
    {
        if (msg.contains("type") && msg.at("type").as_string() == "communication" &&
            msg.contains("action") && msg.at("action").as_string() == "broadcast")
        {
            broadcastPayloads.emplace_back(msg.at("message").as_string().c_str());
        }
    }

    ASSERT_EQ(broadcastPayloads.size(), 2);
    EXPECT_EQ(broadcastPayloads[0], "first message");
    EXPECT_EQ(broadcastPayloads[1], "second message");
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

} // namespace
