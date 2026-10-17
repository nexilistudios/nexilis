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
#include <nexilis/command_spec.hh>
#include <nexilis/room_command_type.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/room_storage.hh>
#include <nexilis/util.hh>

#include <chrono>
#include <cstring>
#include <thread>

using namespace nexilis;

TEST(ClientDataTest, TokenRemainsUsableAfterMove)
{
    client::ClientAPI::ClientData source;
    const std::string firstToken = "first";
    source.setUdpToken(firstToken);
    EXPECT_EQ(source.getUdpToken(), firstToken);
    EXPECT_EQ(source.nextMessageId(), 1);

    client::ClientAPI::ClientData moved(std::move(source));
    EXPECT_EQ(moved.getUdpToken(), firstToken);
    EXPECT_EQ(moved.nextMessageId(), 3);

    source.setUdpToken("second");
    EXPECT_EQ(source.getUdpToken(), "second");

    client::ClientAPI::ClientData assigned;
    assigned = std::move(moved);
    EXPECT_EQ(assigned.getUdpToken(), firstToken);
    EXPECT_EQ(assigned.nextMessageId(), 5);

    moved.setUdpToken("third");
    EXPECT_EQ(moved.getUdpToken(), "third");
}

class CommandSpecTest : public ::testing::Test
{
protected:
    CommandSpec spec;

    void SetUp() override
    {
        auto userPtr = std::make_unique<server::User>(spec.user.getId(), spec.user.getIPAddress());
        server::ClientStorage::add(std::move(userPtr));
    }

    void TearDown() override
    {
        server::ClientStorage::clear();
        server::RoomStorage::clear();
    }
};

TEST_F(CommandSpecTest, Constructs)
{
    EXPECT_EQ(spec.user.getId(), 1);
    EXPECT_EQ(spec.user.getUsername(), "");
    EXPECT_EQ(spec.protocol.getType(), NXProtocol::Type::UNKNOWN);
}

TEST_F(CommandSpecTest, ArgsHelper)
{
    const nx_data data = {0x00, 0x00, 0x01, 'a'};
    auto da = spec.args(data, 7);

    EXPECT_EQ(da.getUser().getId(), 1);
    EXPECT_EQ(da.getMessageId(), 7);
    EXPECT_EQ(da.getData().size(), data.size());
    EXPECT_EQ(da.getData()[3], 'a');
}

TEST_F(CommandSpecTest, PacketWireFormat)
{
    nexilis::client::ClientConfig config;
    nexilis::client::ClientAPI api(config);
    auto bytes = client::Packet::Set::General::username(api, "abc");

    ASSERT_GE(bytes.size(), 6);
    EXPECT_EQ(bytes[0], static_cast<uint8_t>(CommandType::setting));
    EXPECT_EQ(bytes[1], 0);
    EXPECT_EQ(bytes[2], 1);
    EXPECT_EQ(bytes[3], 'a');
    EXPECT_EQ(bytes[4], 'b');
    EXPECT_EQ(bytes[5], 'c');
}

TEST_F(CommandSpecTest, SetUsernameFromRawBytes)
{
    nx_data bytes = {0x00, 0x00, 0x01};
    const char* name = "test_user";
    bytes.insert(bytes.end(), name, name + std::strlen(name));

    EXPECT_EQ(spec.read(bytes), server::CommandResult::success);
}

TEST_F(CommandSpecTest, RejectsCommandsWithoutTwoRequiredBytes)
{
    EXPECT_EQ(spec.read({}), server::CommandResult::invalid_input);
    EXPECT_EQ(spec.read({static_cast<uint8_t>(CommandType::setting)}),
              server::CommandResult::invalid_input);
}

TEST_F(CommandSpecTest, MissingOptionalCommandLevelsDoNotDispatch)
{
    EXPECT_EQ(spec.read({static_cast<uint8_t>(CommandType::setting), 0}),
              server::CommandResult::not_found);
    EXPECT_EQ(spec.read({static_cast<uint8_t>(CommandType::setting), 0, 255}),
              server::CommandResult::not_found);
    EXPECT_EQ(spec.read({static_cast<uint8_t>(CommandType::setting), 1, 0, 0}),
              server::CommandResult::unimplemented);
}

TEST_F(CommandSpecTest, SetUsernameViaPacketApi)
{
    nexilis::client::ClientConfig config;
    nexilis::client::ClientAPI api(config);
    auto bytes = client::Packet::Set::General::username(api, "packet_user");
    EXPECT_EQ(spec.read(bytes), server::CommandResult::success);
}

TEST_F(CommandSpecTest, SetUsernameMultipleTimes)
{
    for (int i = 0; i < 3; i++)
    {
        nx_data bytes = {0x00, 0x00, 0x01};
        bytes.push_back('a' + i);
        EXPECT_EQ(spec.read(bytes), server::CommandResult::success);
    }
}

TEST_F(CommandSpecTest, SetClientIdFailsWithoutRoot)
{
    nx_data bytes = {0x00, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00,
                     0x00, 0x00, 0x00, 0x00};
    EXPECT_EQ(spec.read(bytes), server::CommandResult::unauthorized);
}

TEST_F(CommandSpecTest, UndefinedCommandTypeReturnsError)
{
    // CommandType::undefined (8) explicitly returns CommandResult::error.
    nx_data bytes = {0x08, 0x00, 0x00, 0x00};
    EXPECT_EQ(spec.read(bytes), server::CommandResult::error);
}

TEST_F(CommandSpecTest, PacketAndCommandsAreDifferentTypes)
{
    EXPECT_FALSE((std::is_same_v<client::Packet, server::Commands>));
}

TEST_F(CommandSpecTest, ServerCommandResultEnum)
{
    EXPECT_TRUE(server::checkResult(server::CommandResult::success));
    EXPECT_FALSE(server::checkResult(server::CommandResult::failure));
    EXPECT_FALSE(server::checkResult(server::CommandResult::error));
}

namespace
{
constexpr uint64_t OVERLAP_ROOM_ID = 4242;

server::Room makeOverlapRoom()
{
    RoomData roomData(1, "overlap_room", OVERLAP_ROOM_ID, RoomData::Context::_3D);
    return server::Room(roomData);
}

/// Movement command bytes: room, player_3D, movement, Vector3f, delta.
nx_data player3dMovementBytes(Vector3f movement, float delta)
{
    nx_data bytes = {static_cast<uint8_t>(CommandType::room),
                     static_cast<uint8_t>(RoomCommandType::Root::player_3D),
                     static_cast<uint8_t>(RoomCommandType::PlayerType::movement)};
    auto movementBytes = Util::convertToByteVector(movement);
    bytes.insert(bytes.end(), movementBytes.begin(), movementBytes.end());
    auto deltaBytes = Util::convertToByteVector(delta);
    bytes.insert(bytes.end(), deltaBytes.begin(), deltaBytes.end());
    return bytes;
}
} // namespace

TEST_F(CommandSpecTest, RoomOverlapAllowedByDefaultAndSurvivesMove)
{
    auto room = makeOverlapRoom();
    EXPECT_TRUE(room.isOverlappingAllowed());

    room.setOverlappingAllowed(false);
    server::Room movedRoom(std::move(room));
    EXPECT_FALSE(movedRoom.isOverlappingAllowed());

    movedRoom.setOverlappingAllowed(true);
    server::Room assignedRoom(makeOverlapRoom());
    assignedRoom = std::move(movedRoom);
    EXPECT_TRUE(assignedRoom.isOverlappingAllowed());
}

TEST_F(CommandSpecTest, SetOverlapRequiresRoomMembership)
{
    nx_data bytes = {0x02, 0x00, 0x04, 0x00};
    EXPECT_EQ(spec.read(bytes), server::CommandResult::error);
}

TEST_F(CommandSpecTest, SetOverlapUpdatesRoomViaRawBytes)
{
    auto room = makeOverlapRoom();
    room.joinRoom(spec.user.getId());
    server::RoomStorage::add(std::move(room));
    spec.user.setRoomId(OVERLAP_ROOM_ID);

    nx_data allow = {0x02, 0x00, 0x04, 0x01};
    EXPECT_EQ(spec.read(allow), server::CommandResult::success);
    EXPECT_TRUE(server::RoomStorage::getRoomById(OVERLAP_ROOM_ID)->isOverlappingAllowed());

    nx_data disallow = {0x02, 0x00, 0x04, 0x00};
    EXPECT_EQ(spec.read(disallow), server::CommandResult::success);
    EXPECT_FALSE(server::RoomStorage::getRoomById(OVERLAP_ROOM_ID)->isOverlappingAllowed());
}

TEST_F(CommandSpecTest, SetOverlapWireFormatFromPacketApi)
{
    client::ClientConfig config;
    client::ClientAPI api(config);
    api.setClientId(1);

    auto bytes = client::Packet::Room::Management::setOverlap(api, false);
    ASSERT_EQ(bytes.size(), 16 + 3 + 1); // client+message ids + header + payload
    EXPECT_EQ(bytes[16], static_cast<uint8_t>(CommandType::room));
    EXPECT_EQ(bytes[17], static_cast<uint8_t>(RoomCommandType::Root::management));
    EXPECT_EQ(bytes[18], static_cast<uint8_t>(RoomCommandType::Management::set_overlap));
    EXPECT_EQ(bytes[19], 0);
}

TEST_F(CommandSpecTest, Player3DMovementBlockedWhenOverlappingDisabled)
{
    auto room = makeOverlapRoom();
    room.joinRoom(spec.user.getId());

    auto otherPtr = std::make_unique<server::User>(2, "127.0.0.2");
    otherPtr->setRoomId(OVERLAP_ROOM_ID);
    otherPtr->getObject3D().setPosition(Vector3f(0.5f, 0.5f, 0.5f));
    server::ClientStorage::add(std::move(otherPtr));

    room.joinRoom(2);
    server::RoomStorage::add(std::move(room));
    spec.user.setRoomId(OVERLAP_ROOM_ID);

    // Disable overlapping via the management message.
    nx_data disallow = {0x02, 0x00, 0x04, 0x00};
    ASSERT_EQ(spec.read(disallow), server::CommandResult::success);

    // Both clients are within each other's bounding boxes, so the whole
    // movement stays inside the other client: nothing may go through.
    float delta = 0.05f;
    ASSERT_EQ(spec.read(player3dMovementBytes(Vector3f(0.4f, 0.f, 0.f), delta)),
              server::CommandResult::success);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    auto pos = spec.user.getObject3D().getPosition();
    EXPECT_FLOAT_EQ(pos.x, 0.0f);
    EXPECT_FLOAT_EQ(pos.y, 0.0f);
    EXPECT_FLOAT_EQ(pos.z, 0.0f);

    // Allow overlapping again: the same movement now goes through.
    nx_data allow = {0x02, 0x00, 0x04, 0x01};
    ASSERT_EQ(spec.read(allow), server::CommandResult::success);

    ASSERT_EQ(spec.read(player3dMovementBytes(Vector3f(0.4f, 0.f, 0.f), delta)),
              server::CommandResult::success);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    pos = spec.user.getObject3D().getPosition();
    EXPECT_GT(pos.x, 0.0f);
}
