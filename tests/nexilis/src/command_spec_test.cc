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
        // Room creation acknowledges the action by sending a message back to
        // the client; give the test user a no-op send callback so the dispatch
        // does not fail with failed_room_send.
        spec.user.setBoostTCPSend([](const nx_data&) {});

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

server::Room makeOverlapRoom(uint64_t creatorId = 1)
{
    RoomData roomData(creatorId, "overlap_room", OVERLAP_ROOM_ID, RoomData::Context::_3D);
    return server::Room(roomData);
}

/// Room create bytes: room, management, create, context, max size, name.
nx_data roomCreateBytes(const std::string& name)
{
    nx_data bytes = {static_cast<uint8_t>(CommandType::room),
                     static_cast<uint8_t>(RoomCommandType::Root::management),
                     static_cast<uint8_t>(RoomCommandType::Management::create),
                     0};
    auto maxSize = Util::convertToByteVector(static_cast<uint16_t>(30));
    bytes.insert(bytes.end(), maxSize.begin(), maxSize.end());
    bytes.insert(bytes.end(), name.begin(), name.end());
    return bytes;
}

/// Room remove bytes: room, management, remove, room id.
nx_data roomRemoveBytes(uint64_t roomId)
{
    nx_data bytes = {static_cast<uint8_t>(CommandType::room),
                     static_cast<uint8_t>(RoomCommandType::Root::management),
                     static_cast<uint8_t>(RoomCommandType::Management::remove)};
    auto idBytes = Util::convertToByteVector(roomId);
    bytes.insert(bytes.end(), idBytes.begin(), idBytes.end());
    return bytes;
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
    room.setClientRoomRemovalAllowed(true);
    server::Room movedRoom(std::move(room));
    EXPECT_FALSE(movedRoom.isOverlappingAllowed());
    EXPECT_TRUE(movedRoom.isClientRoomRemovalAllowed());

    movedRoom.setOverlappingAllowed(true);
    server::Room assignedRoom(makeOverlapRoom());
    assignedRoom = std::move(movedRoom);
    EXPECT_TRUE(assignedRoom.isOverlappingAllowed());
    EXPECT_TRUE(assignedRoom.isClientRoomRemovalAllowed());
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

TEST_F(CommandSpecTest, SetOverlapRejectsNonCreator)
{
    auto room = makeOverlapRoom(2);
    room.joinRoom(spec.user.getId());
    server::RoomStorage::add(std::move(room));
    spec.user.setRoomId(OVERLAP_ROOM_ID);

    nx_data disallow = {0x02, 0x00, 0x04, 0x00};
    EXPECT_EQ(spec.read(disallow), server::CommandResult::unauthorized);
    EXPECT_TRUE(server::RoomStorage::getRoomById(OVERLAP_ROOM_ID)->isOverlappingAllowed());
}

TEST_F(CommandSpecTest, RoomRemovalIsDisabledByDefault)
{
    auto room = makeOverlapRoom();
    room.joinRoom(spec.user.getId());
    server::RoomStorage::add(std::move(room));
    spec.user.setRoomId(OVERLAP_ROOM_ID);

    EXPECT_EQ(spec.read(roomRemoveBytes(OVERLAP_ROOM_ID)),
              server::CommandResult::unauthorized);
    EXPECT_TRUE(server::RoomStorage::contains(OVERLAP_ROOM_ID));
}

TEST_F(CommandSpecTest, RoomRemovalRejectsShortPayload)
{
    nx_data bytes = {0x02, 0x00, 0x03, 0x01, 0x02};
    EXPECT_EQ(spec.read(bytes), server::CommandResult::invalid_input);
}

TEST_F(CommandSpecTest, RoomRemovalWorksWhenRoomOptsIn)
{
    auto room = makeOverlapRoom();
    room.setClientRoomRemovalAllowed(true);
    room.joinRoom(spec.user.getId());
    server::RoomStorage::add(std::move(room));
    spec.user.setRoomId(OVERLAP_ROOM_ID);

    EXPECT_EQ(spec.read(roomRemoveBytes(OVERLAP_ROOM_ID)),
              server::CommandResult::success);
    EXPECT_FALSE(server::RoomStorage::contains(OVERLAP_ROOM_ID));
}

TEST_F(CommandSpecTest, RoomCreationRejectsTruncatedAndOversizedRequests)
{
    unsigned sends = 0;
    spec.user.setBoostTCPSend([&](const nx_data&)
                              { ++sends; });
    const auto valid = roomCreateBytes("room");
    // Complete command bytes, followed by zero through three payload bytes.
    // A three-byte payload has context/capacity but no room name.
    for (std::size_t size = 3; size <= 6; ++size)
    {
        auto truncated = valid;
        truncated.resize(size);
        EXPECT_EQ(spec.read(truncated), server::CommandResult::invalid_input);
    }
    EXPECT_EQ(spec.read(roomCreateBytes(std::string(65, 'a'))),
              server::CommandResult::invalid_input);
    EXPECT_EQ(server::RoomStorage::getRoomCount(), 0);
    EXPECT_EQ(sends, 0);
}

TEST_F(CommandSpecTest, RoomCreationValidatesContextAndCapacity)
{
    auto invalidContext = roomCreateBytes("room");
    invalidContext[3] = 2;
    EXPECT_EQ(spec.read(invalidContext), server::CommandResult::invalid_input);
    for (uint16_t capacity : {uint16_t{0}, uint16_t{65}})
    {
        auto bytes = roomCreateBytes("room");
        auto encoded = Util::convertToByteVector(capacity);
        bytes[4] = encoded[0];
        bytes[5] = encoded[1];
        EXPECT_EQ(spec.read(bytes), server::CommandResult::invalid_input);
    }
    EXPECT_EQ(spec.read(roomCreateBytes(" ")), server::CommandResult::invalid_input);
    EXPECT_EQ(server::RoomStorage::getRoomCount(), 0);
}

TEST_F(CommandSpecTest, RoomCreationAcceptsNameAndCapacityBoundaries)
{
    for (uint16_t capacity : {uint16_t{1}, uint16_t{64}})
    {
        const std::string name(capacity, 'a');
        auto bytes = roomCreateBytes(name);
        auto encoded = Util::convertToByteVector(capacity);
        bytes[4] = encoded[0];
        bytes[5] = encoded[1];
        bytes[3] = capacity == 1 ? 0 : 1; // both valid room contexts
        ASSERT_EQ(spec.read(bytes), server::CommandResult::success);
        const auto rooms = server::RoomStorage::getAllRooms();
        ASSERT_FALSE(rooms.empty());
        const auto& room = rooms.back();
        EXPECT_EQ(room->getName(), name);
        EXPECT_EQ(room->getMaxSize(), capacity);
        EXPECT_EQ(static_cast<uint8_t>(room->getContext()), bytes[3]);
    }
    EXPECT_EQ(server::RoomStorage::getRoomCount(), 2);
}

TEST_F(CommandSpecTest, RoomCreationRejectsWhenServerLimitReached)
{
    server::RoomStorage::add(makeOverlapRoom());
    auto& limits = spec.command.getServerConfig().getRoomLimits();
    limits.setMaxRooms(1);

    EXPECT_EQ(spec.read(roomCreateBytes("extra")), server::CommandResult::failure);
    EXPECT_EQ(server::RoomStorage::getRoomCount(), 1);

    limits.setMaxRooms(2);
    EXPECT_EQ(spec.read(roomCreateBytes("extra")), server::CommandResult::success);
    EXPECT_EQ(server::RoomStorage::getRoomCount(), 2);
}

TEST_F(CommandSpecTest, RoomCreationRejectsWhenClientLimitReached)
{
    server::RoomStorage::add(makeOverlapRoom());
    auto& limits = spec.command.getServerConfig().getRoomLimits();
    limits.setMaxRooms(10);
    limits.setMaxRoomsPerClient(1);

    EXPECT_EQ(spec.read(roomCreateBytes("extra")), server::CommandResult::failure);
    EXPECT_EQ(server::RoomStorage::getRoomCount(), 1);

    limits.setMaxRoomsPerClient(2);
    EXPECT_EQ(spec.read(roomCreateBytes("extra")), server::CommandResult::success);
    EXPECT_EQ(server::RoomStorage::getRoomCount(), 2);
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
    auto pos = spec.user.getObject3D().getPosition();
    EXPECT_FLOAT_EQ(pos.x, 0.0f);
    EXPECT_FLOAT_EQ(pos.y, 0.0f);
    EXPECT_FLOAT_EQ(pos.z, 0.0f);

    // Allow overlapping again: the same movement now goes through.
    nx_data allow = {0x02, 0x00, 0x04, 0x01};
    ASSERT_EQ(spec.read(allow), server::CommandResult::success);

    ASSERT_EQ(spec.read(player3dMovementBytes(Vector3f(0.4f, 0.f, 0.f), delta)),
              server::CommandResult::success);
    pos = spec.user.getObject3D().getPosition();
    EXPECT_GT(pos.x, 0.0f);
}
