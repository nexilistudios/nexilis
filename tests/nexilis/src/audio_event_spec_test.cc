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
#include <nexilis/client/packet.hh>
#include <nexilis/json.hh>
#include <nexilis/room_command_type.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/command/command_result.hh>
#include <nexilis/server/room_storage.hh>
#include <nexilis/server/user.hh>
#include <nexilis/util.hh>

#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>

#include <memory>
#include <string>
#include <vector>

using namespace nexilis;

namespace
{

constexpr uint64_t ROOM_ID = 888;
constexpr uint64_t SENDER_ID = 10;
constexpr uint64_t OTHER_ID = 20;
constexpr uint8_t SOUND = 3;

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

/// Audio event command bytes: room / player_3D / audio_event / sound / Vector3f.
nx_data audioEventBytes(uint8_t sound, Vector3f position)
{
    nx_data bytes{static_cast<uint8_t>(CommandType::room),
                  static_cast<uint8_t>(RoomCommandType::Root::player_3D),
                  static_cast<uint8_t>(RoomCommandType::PlayerType::audio_event)};
    bytes.emplace_back(sound);
    auto positionBytes = Util::convertToByteVector(position);
    bytes.insert(bytes.end(), positionBytes.begin(), positionBytes.end());
    return bytes;
}

class AudioEventSpecTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        server::RoomStorage::clear();
        server::ClientStorage::clear();

        auto sender = std::make_unique<server::User>(SENDER_ID, "127.0.0.1");
        auto other = std::make_unique<server::User>(OTHER_ID, "127.0.0.2");
        m_sender = sender.get();
        m_other = other.get();
        server::ClientStorage::add(std::move(sender));
        server::ClientStorage::add(std::move(other));
    }

    void TearDown() override
    {
        server::RoomStorage::clear();
        server::ClientStorage::clear();
    }

    server::ServerConfig settings;
    BoostTCPServerProtocol protocol;
    server::Command command{settings};

    server::User* m_sender = nullptr;
    server::User* m_other = nullptr;

    /// Put the sender (and optionally the other client) into a stored room.
    /// Every room member needs an outbound callback so the room broadcast
    /// reports success.
    void setUpRoom(bool includeOther = true)
    {
        server::RoomStorage::add(server::Room(RoomData(SENDER_ID, "audio_room", ROOM_ID)));
        auto* room = server::RoomStorage::getRoomById(ROOM_ID);
        room->joinRoom(SENDER_ID);
        m_sender->setRoomId(ROOM_ID);
        m_sender->setBoostTCPSend([](const nx_data&) {});

        if (includeOther)
        {
            room->joinRoom(OTHER_ID);
            m_other->setRoomId(ROOM_ID);
            m_other->setBoostTCPSend([](const nx_data&) {});
        }
    }

    /// Collect all JSON messages sent to a client via its boost TCP callback.
    std::vector<boost::json::object> receivedMessages(server::User& client)
    {
        std::vector<boost::json::object> messages;
        client.setBoostTCPSend([&messages](const nx_data& data)
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

TEST_F(AudioEventSpecTest, UserNotInRoomFails)
{
    ASSERT_EQ(command.read(audioEventBytes(SOUND, Vector3f(1.0f, 2.0f, 3.0f)),
                           *m_sender, protocol, 1),
              server::CommandResult::error);
}

TEST_F(AudioEventSpecTest, InsufficientPayloadFails)
{
    setUpRoom();
    nx_data bytes{static_cast<uint8_t>(CommandType::room),
                  static_cast<uint8_t>(RoomCommandType::Root::player_3D),
                  static_cast<uint8_t>(RoomCommandType::PlayerType::audio_event),
                  0x00, 0x00, 0x00};
    ASSERT_EQ(command.read(bytes, *m_sender, protocol, 1),
              server::CommandResult::invalid_input);
}

TEST_F(AudioEventSpecTest, RoomNotFoundFails)
{
    // The sender claims to be in a room that is not stored on the server.
    m_sender->setRoomId(ROOM_ID);
    ASSERT_EQ(command.read(audioEventBytes(SOUND, Vector3f(1.0f, 2.0f, 3.0f)),
                           *m_sender, protocol, 1),
              server::CommandResult::failure);
}

TEST_F(AudioEventSpecTest, RelaysAudioEventToRoomMembers)
{
    setUpRoom();
    auto messages = receivedMessages(*m_other);

    const Vector3f position(4.0f, 5.0f, 6.0f);
    ASSERT_EQ(command.read(audioEventBytes(SOUND, position), *m_sender, protocol, 7),
              server::CommandResult::success);

    ASSERT_EQ(messages.size(), 1);
    const auto& msg = messages[0];
    EXPECT_EQ(msg.at("command").as_string(), "room");
    EXPECT_EQ(msg.at("type").as_string(), "player_3D");
    EXPECT_EQ(msg.at("action").as_string(), "audio_event");
    EXPECT_EQ(Util::toUint64(msg.at("room_id")), ROOM_ID);
    EXPECT_EQ(Util::toUint64(msg.at("client_id")), SENDER_ID);
    EXPECT_EQ(Util::toUint64(msg.at("sound")), SOUND);
    EXPECT_DOUBLE_EQ(msg.at("x").as_double(), 4.0);
    EXPECT_DOUBLE_EQ(msg.at("y").as_double(), 5.0);
    EXPECT_DOUBLE_EQ(msg.at("z").as_double(), 6.0);
    EXPECT_EQ(Util::toUint64(msg.at("callback")), 7);
}

TEST_F(AudioEventSpecTest, PacketWireFormat)
{
    client::ClientConfig config;
    client::ClientAPI api(config);
    api.setClientId(1);

    auto bytes = client::Packet::Room::Player3D::audioEvent(api, SOUND, Vector3f(1.0f, 2.0f, 3.0f));
    ASSERT_EQ(bytes.size(), 16 + 3 + 13); // client+message ids + header + sound + Vector3f
    EXPECT_EQ(bytes[16], static_cast<uint8_t>(CommandType::room));
    EXPECT_EQ(bytes[17], static_cast<uint8_t>(RoomCommandType::Root::player_3D));
    EXPECT_EQ(bytes[18], static_cast<uint8_t>(RoomCommandType::PlayerType::audio_event));
    EXPECT_EQ(bytes[19], SOUND);
}

TEST_F(AudioEventSpecTest, ClientParsesAndQueuesAudioEvent)
{
    client::ClientConfig config;
    client::ClientAPI api(config);
    api.setClientId(1);

    boost::json::object json;
    json["command"] = "room";
    json["type"] = "player_3D";
    json["callback"] = 0;
    json["action"] = "audio_event";
    json["room_id"] = ROOM_ID;
    json["client_id"] = SENDER_ID;
    json["sound"] = static_cast<uint64_t>(SOUND);
    json["x"] = 10.5;
    json["y"] = -3.25;
    json["z"] = 7.75;

    auto serialized = boost::json::serialize(json);
    nx_data bytes(serialized.begin(), serialized.end());
    bytes.emplace_back('\n');

    ASSERT_EQ(api.readMessage(bytes), client::ReadResult::success);

    auto events = api.consumeAudioEvents();
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].client_id, SENDER_ID);
    EXPECT_EQ(events[0].sound, SOUND);
    EXPECT_FLOAT_EQ(events[0].x, 10.5f);
    EXPECT_FLOAT_EQ(events[0].y, -3.25f);
    EXPECT_FLOAT_EQ(events[0].z, 7.75f);
}

TEST_F(AudioEventSpecTest, ClientParsesServerAnimationEvent)
{
    client::ClientConfig config;
    client::ClientAPI api(config);
    api.setClientId(1);

    boost::json::object json;
    json["command"] = "room";
    json["type"] = "player_3D";
    json["callback"] = 0;
    json["action"] = "animation";
    json["room_id"] = ROOM_ID;
    json["client_id"] = SENDER_ID;
    json["state"] = "reloading";
    json["weapon"] = "ak47";
    json["duration"] = 2.5;

    auto serialized = boost::json::serialize(json);
    nx_data bytes(serialized.begin(), serialized.end());
    bytes.emplace_back('\n');

    ASSERT_EQ(api.readMessage(bytes), client::ReadResult::success);
    auto events = api.consumeAnimationEvents();
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].room_id, ROOM_ID);
    EXPECT_EQ(events[0].client_id, SENDER_ID);
    EXPECT_EQ(events[0].state, "reloading");
    EXPECT_EQ(events[0].weapon, "ak47");
    EXPECT_FLOAT_EQ(events[0].duration, 2.5f);
    EXPECT_TRUE(api.consumeAnimationEvents().empty());
}

} // namespace
