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
#include <nexilis/room_command_type.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/gameplay_limits.hh>
#include <nexilis/server/movement.hh>
#include <nexilis/server/room_storage.hh>

#include <boost/json/serialize.hpp>

#include <algorithm>

using namespace nexilis;

namespace
{

constexpr uint64_t ROOM_ID = 777;
constexpr uint64_t SENDER_ID = 10;
constexpr uint64_t TARGET_ID = 20;
constexpr uint64_t OBJECT_ID = 3400;
constexpr uint64_t MESSAGE_ID = 7;
constexpr float DEFAULT_HEALTH = 100.0f;

/// Minimal server protocol that routes outbound traffic through the user's
/// boost TCP send callback.
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

/// Room command bytes: room / player_3D / shoot / targetId / damage.
nx_data shootBytes(uint64_t targetId, float damage)
{
    nx_data bytes{static_cast<uint8_t>(CommandType::room),
                  static_cast<uint8_t>(RoomCommandType::Root::player_3D),
                  static_cast<uint8_t>(RoomCommandType::PlayerType::shoot)};
    auto targetBytes = Util::convertToByteVector(targetId);
    bytes.insert(bytes.end(), targetBytes.begin(), targetBytes.end());
    auto damageBytes = Util::convertToByteVector(damage);
    bytes.insert(bytes.end(), damageBytes.begin(), damageBytes.end());
    return bytes;
}

/// Room command bytes: room / player_3D / set_team / team name.
nx_data setTeamBytes(const std::string& team)
{
    nx_data bytes{static_cast<uint8_t>(CommandType::room),
                  static_cast<uint8_t>(RoomCommandType::Root::player_3D),
                  static_cast<uint8_t>(RoomCommandType::PlayerType::set_team)};
    auto teamBytes = Util::convertToByteVector(team);
    bytes.insert(bytes.end(), teamBytes.begin(), teamBytes.end());
    return bytes;
}

/// Room command bytes: room / object_2D / create_moving / startingPosition /
/// dimensions / movement / deltaTime / movementType / filepath.
nx_data createMoving2DBytes(Vector2f startingPosition, Vector2f dimensions, Vector2f movement,
                            float deltaTime, MovementType type, const std::string& filepath)
{
    nx_data bytes{static_cast<uint8_t>(CommandType::room),
                  static_cast<uint8_t>(RoomCommandType::Root::object_2D),
                  static_cast<uint8_t>(RoomCommandType::ObjectType::create_moving)};
    auto startBytes = Util::convertToByteVector(startingPosition);
    bytes.insert(bytes.end(), startBytes.begin(), startBytes.end());
    auto dimsBytes = Util::convertToByteVector(dimensions);
    bytes.insert(bytes.end(), dimsBytes.begin(), dimsBytes.end());
    auto moveBytes = Util::convertToByteVector(movement);
    bytes.insert(bytes.end(), moveBytes.begin(), moveBytes.end());
    auto deltaBytes = Util::convertToByteVector(deltaTime);
    bytes.insert(bytes.end(), deltaBytes.begin(), deltaBytes.end());
    bytes.emplace_back(static_cast<uint8_t>(type));
    auto fileBytes = Util::convertToByteVector(filepath);
    bytes.insert(bytes.end(), fileBytes.begin(), fileBytes.end());
    return bytes;
}

/// Room command bytes: room / player_3D / position / position vector.
nx_data player3DPositionBytes(const Vector3f& position)
{
    nx_data bytes{static_cast<uint8_t>(CommandType::room),
                  static_cast<uint8_t>(RoomCommandType::Root::player_3D),
                  static_cast<uint8_t>(RoomCommandType::PlayerType::position)};
    auto vectorBytes = Util::convertToByteVector(position);
    bytes.insert(bytes.end(), vectorBytes.begin(), vectorBytes.end());
    return bytes;
}

/// Room command bytes: room / player_3D / dimension / dimensions vector.
nx_data player3DDimensionBytes(const Vector3f& dimensions)
{
    nx_data bytes{static_cast<uint8_t>(CommandType::room),
                  static_cast<uint8_t>(RoomCommandType::Root::player_3D),
                  static_cast<uint8_t>(RoomCommandType::PlayerType::dimension)};
    auto vectorBytes = Util::convertToByteVector(dimensions);
    bytes.insert(bytes.end(), vectorBytes.begin(), vectorBytes.end());
    return bytes;
}

/// Room command bytes: room / player_2D / position / position vector.
nx_data player2DPositionBytes(const Vector2f& position)
{
    nx_data bytes{static_cast<uint8_t>(CommandType::room),
                  static_cast<uint8_t>(RoomCommandType::Root::player_2D),
                  static_cast<uint8_t>(RoomCommandType::PlayerType::position)};
    auto vectorBytes = Util::convertToByteVector(position);
    bytes.insert(bytes.end(), vectorBytes.begin(), vectorBytes.end());
    return bytes;
}

/// Room command bytes: room / player_2D / dimension / dimensions vector.
nx_data player2DDimensionBytes(const Vector2f& dimensions)
{
    nx_data bytes{static_cast<uint8_t>(CommandType::room),
                  static_cast<uint8_t>(RoomCommandType::Root::player_2D),
                  static_cast<uint8_t>(RoomCommandType::PlayerType::dimension)};
    auto vectorBytes = Util::convertToByteVector(dimensions);
    bytes.insert(bytes.end(), vectorBytes.begin(), vectorBytes.end());
    return bytes;
}

/// Room command bytes: room / player_2D / movement / movement / deltaTime.
nx_data player2DMovementBytes(const Vector2f& movement, float deltaTime)
{
    nx_data bytes{static_cast<uint8_t>(CommandType::room),
                  static_cast<uint8_t>(RoomCommandType::Root::player_2D),
                  static_cast<uint8_t>(RoomCommandType::PlayerType::movement)};
    auto moveBytes = Util::convertToByteVector(movement);
    bytes.insert(bytes.end(), moveBytes.begin(), moveBytes.end());
    auto deltaBytes = Util::convertToByteVector(deltaTime);
    bytes.insert(bytes.end(), deltaBytes.begin(), deltaBytes.end());
    return bytes;
}

/// Room command bytes: room / player_3D / movement / movement / deltaTime.
nx_data player3DMovementBytes(const Vector3f& movement, float deltaTime)
{
    nx_data bytes{static_cast<uint8_t>(CommandType::room),
                  static_cast<uint8_t>(RoomCommandType::Root::player_3D),
                  static_cast<uint8_t>(RoomCommandType::PlayerType::movement)};
    auto moveBytes = Util::convertToByteVector(movement);
    bytes.insert(bytes.end(), moveBytes.begin(), moveBytes.end());
    auto deltaBytes = Util::convertToByteVector(deltaTime);
    bytes.insert(bytes.end(), deltaBytes.begin(), deltaBytes.end());
    return bytes;
}

/// Room command bytes: room / object_2D / create / position / dimensions / filepath.
nx_data createObject2DBytes(const Vector2f& position, const Vector2f& dimensions, const std::string& filepath)
{
    nx_data bytes{static_cast<uint8_t>(CommandType::room),
                  static_cast<uint8_t>(RoomCommandType::Root::object_2D),
                  static_cast<uint8_t>(RoomCommandType::ObjectType::create)};
    auto posBytes = Util::convertToByteVector(position);
    bytes.insert(bytes.end(), posBytes.begin(), posBytes.end());
    auto dimsBytes = Util::convertToByteVector(dimensions);
    bytes.insert(bytes.end(), dimsBytes.begin(), dimsBytes.end());
    auto fileBytes = Util::convertToByteVector(filepath);
    bytes.insert(bytes.end(), fileBytes.begin(), fileBytes.end());
    return bytes;
}

/// Room command bytes: room / object_2D / destroy / objectId.
nx_data destroyObject2DBytes(uint64_t objectId)
{
    nx_data bytes{static_cast<uint8_t>(CommandType::room),
                  static_cast<uint8_t>(RoomCommandType::Root::object_2D),
                  static_cast<uint8_t>(RoomCommandType::ObjectType::destroy)};
    auto idBytes = Util::convertToByteVector(objectId);
    bytes.insert(bytes.end(), idBytes.begin(), idBytes.end());
    return bytes;
}

/// Room command bytes: room / object_2D / move / objectId / move offset.
nx_data moveObject2DBytes(uint64_t objectId, const Vector2f& offset)
{
    nx_data bytes{static_cast<uint8_t>(CommandType::room),
                  static_cast<uint8_t>(RoomCommandType::Root::object_2D),
                  static_cast<uint8_t>(RoomCommandType::ObjectType::move)};
    auto idBytes = Util::convertToByteVector(objectId);
    bytes.insert(bytes.end(), idBytes.begin(), idBytes.end());
    auto offsetBytes = Util::convertToByteVector(offset);
    bytes.insert(bytes.end(), offsetBytes.begin(), offsetBytes.end());
    return bytes;
}

/// Room command bytes: room / object_3D / create / position / dimensions / filepath.
nx_data createObject3DBytes(const Vector3f& position, const Vector3f& dimensions, const std::string& filepath)
{
    nx_data bytes{static_cast<uint8_t>(CommandType::room),
                  static_cast<uint8_t>(RoomCommandType::Root::object_3D),
                  static_cast<uint8_t>(RoomCommandType::ObjectType::create)};
    auto posBytes = Util::convertToByteVector(position);
    bytes.insert(bytes.end(), posBytes.begin(), posBytes.end());
    auto dimsBytes = Util::convertToByteVector(dimensions);
    bytes.insert(bytes.end(), dimsBytes.begin(), dimsBytes.end());
    auto fileBytes = Util::convertToByteVector(filepath);
    bytes.insert(bytes.end(), fileBytes.begin(), fileBytes.end());
    return bytes;
}

/// Room command bytes: room / object_3D / destroy / objectId.
nx_data destroyObject3DBytes(uint64_t objectId)
{
    nx_data bytes{static_cast<uint8_t>(CommandType::room),
                  static_cast<uint8_t>(RoomCommandType::Root::object_3D),
                  static_cast<uint8_t>(RoomCommandType::ObjectType::destroy)};
    auto idBytes = Util::convertToByteVector(objectId);
    bytes.insert(bytes.end(), idBytes.begin(), idBytes.end());
    return bytes;
}

/// Room command bytes: room / object_3D / move / objectId / move offset.
nx_data moveObject3DBytes(uint64_t objectId, const Vector3f& offset)
{
    nx_data bytes{static_cast<uint8_t>(CommandType::room),
                  static_cast<uint8_t>(RoomCommandType::Root::object_3D),
                  static_cast<uint8_t>(RoomCommandType::ObjectType::move)};
    auto idBytes = Util::convertToByteVector(objectId);
    bytes.insert(bytes.end(), idBytes.begin(), idBytes.end());
    auto offsetBytes = Util::convertToByteVector(offset);
    bytes.insert(bytes.end(), offsetBytes.begin(), offsetBytes.end());
    return bytes;
}

/// Room command bytes: room / object_3D / create_moving / startingPosition /
/// dimensions / movement / deltaTime / movementType / filepath.
nx_data createMoving3DBytes(Vector3f startingPosition, Vector3f dimensions, Vector3f movement,
                            float deltaTime, MovementType type, const std::string& filepath)
{
    nx_data bytes{static_cast<uint8_t>(CommandType::room),
                  static_cast<uint8_t>(RoomCommandType::Root::object_3D),
                  static_cast<uint8_t>(RoomCommandType::ObjectType::create_moving)};
    auto startBytes = Util::convertToByteVector(startingPosition);
    bytes.insert(bytes.end(), startBytes.begin(), startBytes.end());
    auto dimsBytes = Util::convertToByteVector(dimensions);
    bytes.insert(bytes.end(), dimsBytes.begin(), dimsBytes.end());
    auto moveBytes = Util::convertToByteVector(movement);
    bytes.insert(bytes.end(), moveBytes.begin(), moveBytes.end());
    auto deltaBytes = Util::convertToByteVector(deltaTime);
    bytes.insert(bytes.end(), deltaBytes.begin(), deltaBytes.end());
    bytes.emplace_back(static_cast<uint8_t>(type));
    auto fileBytes = Util::convertToByteVector(filepath);
    bytes.insert(bytes.end(), fileBytes.begin(), fileBytes.end());
    return bytes;
}

/// A "getting"/"info_rooms" JSON message with a single empty room.
boost::json::object infoRoomsMessage()
{
    boost::json::object roomObj;
    roomObj["name"] = "gameplay_room";
    roomObj["max_size"] = 10;
    roomObj["room_id"] = ROOM_ID;
    roomObj["creator_id"] = SENDER_ID;
    roomObj["context"] = static_cast<uint64_t>(RoomData::Context::_3D);

    boost::json::object json;
    json["command"] = "getting";
    json["type"] = "info_rooms";
    json["callback"] = 0;
    json["rooms"] = boost::json::array{std::move(roomObj)};
    return json;
}

/// A "getting"/"info_rooms" JSON message with a single room whose client list
/// contains one session per entry in `clients` (the shape the server uses when
/// polling existing members).
boost::json::object infoRoomsWithClientsMessage(const std::vector<uint64_t>& clients)
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
    roomObj["name"] = "gameplay_room";
    roomObj["max_size"] = 10;
    roomObj["room_id"] = ROOM_ID;
    roomObj["creator_id"] = SENDER_ID;
    roomObj["context"] = static_cast<uint64_t>(RoomData::Context::_3D);
    if (!clientArray.empty())
        roomObj["clients"] = std::move(clientArray);

    boost::json::object json;
    json["command"] = "getting";
    json["type"] = "info_rooms";
    json["callback"] = 0;
    json["rooms"] = boost::json::array{std::move(roomObj)};
    return json;
}

/// Deliver a JSON command message to the client as raw wire bytes.
client::ReadResult readClientMessage(client::ClientAPI& api, const boost::json::object& json)
{
    auto serialized = boost::json::serialize(json);
    nx_data bytes(serialized.begin(), serialized.end());
    bytes.emplace_back('\n');
    return api.readMessage(bytes);
}

/// Parse wire JSON bytes (dropping the trailing newline, if any) to an object.
boost::json::object parseMessage(const nx_data& data)
{
    nx_data jsonData = data;
    if (!jsonData.empty() && jsonData.back() == '\n')
    {
        jsonData.pop_back();
    }
    return Json::convertToJSON(jsonData);
}

class GameplaySpecTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        server::RoomStorage::clear();
        server::ClientStorage::clear();

        // Legacy tests fire shots back-to-back; disable the default
        // shooting rate limit so it does not interfere.
        command.getServerConfig().getGameplayLimits().setMaxShootingRate(0.0f);

        auto sender = std::make_unique<server::User>(SENDER_ID, "127.0.0.1");
        auto target = std::make_unique<server::User>(TARGET_ID, "127.0.0.2");
        m_sender = sender.get();
        m_target = target.get();
        server::ClientStorage::add(std::move(sender));
        server::ClientStorage::add(std::move(target));
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
    server::User* m_target = nullptr;

    /// Put both clients in a stored room. Every member gets a no-op outbound
    /// callback so broadcasts report success.
    void setUpRoom()
    {
        server::RoomStorage::add(server::Room(RoomData(SENDER_ID, "gameplay_room", ROOM_ID)));
        auto* room = server::RoomStorage::getRoomById(ROOM_ID);
        room->joinRoom(SENDER_ID);
        m_sender->setRoomId(ROOM_ID);
        m_sender->setBoostTCPSend([](const nx_data&) {});

        room->joinRoom(TARGET_ID);
        m_target->setRoomId(ROOM_ID);
        m_target->setBoostTCPSend([](const nx_data&) {});

        // Client position requests are disabled by default, so the legacy
        // position tests must opt in explicitly.
        room->setClientPositionAllowed(true);
    }

    /// Put the shooter and the target on opposing teams. The shoot command only
    /// accepts damage dealt to a member of another team.
    void setUpOpposingTeams()
    {
        auto* room = server::RoomStorage::getRoomById(ROOM_ID);
        room->setPlayerTeam(SENDER_ID, "Terrorist");
        room->setPlayerTeam(TARGET_ID, "Counter Terrorist");
    }

    /// Drop the target to `health` without going through the shoot command.
    /// A single shot is capped at 35 damage, so a full health target never dies
    /// in one hit and the kill tests need a weakened target first.
    void setTargetHealth(float health)
    {
        server::RoomStorage::getRoomById(ROOM_ID)->damagePlayer(TARGET_ID, DEFAULT_HEALTH - health);
    }

    server::CommandResult read(const nx_data& bytes, server::User& user)
    {
        return command.read(bytes, user, protocol, MESSAGE_ID);
    }

    /// Collect all JSON messages sent to a client via its boost TCP callback.
    std::vector<boost::json::object> receivedMessages(server::User& client)
    {
        std::vector<boost::json::object> messages;
        client.setBoostTCPSend([&messages](const nx_data& data)
                               { messages.emplace_back(parseMessage(data)); });
        return messages;
    }
};

// ---------------------------------------------------------------------------
// Shoot / damage
// ---------------------------------------------------------------------------

TEST_F(GameplaySpecTest, ShootFailsWhenNotInRoom)
{
    // The shooter has not joined any room (room id stays 0).
    EXPECT_EQ(read(shootBytes(TARGET_ID, 25.0f), *m_sender), server::CommandResult::error);
}

TEST_F(GameplaySpecTest, ShootInsufficientPayloadFails)
{
    setUpRoom();
    nx_data bytes{static_cast<uint8_t>(CommandType::room),
                  static_cast<uint8_t>(RoomCommandType::Root::player_3D),
                  static_cast<uint8_t>(RoomCommandType::PlayerType::shoot)};
    EXPECT_EQ(read(bytes, *m_sender), server::CommandResult::invalid_input);
}

TEST_F(GameplaySpecTest, ShootRoomNotFoundFails)
{
    // The shooter claims to be in a room that is not stored on the server.
    m_sender->setRoomId(ROOM_ID);
    EXPECT_EQ(read(shootBytes(TARGET_ID, 25.0f), *m_sender), server::CommandResult::failure);
}

TEST_F(GameplaySpecTest, ShootTargetNotInRoomFails)
{
    setUpRoom();
    // Only the sender is a member of the room; the target is not.
    auto* room = server::RoomStorage::getRoomById(ROOM_ID);
    room->leaveRoom(TARGET_ID);

    EXPECT_EQ(read(shootBytes(TARGET_ID, 25.0f), *m_sender), server::CommandResult::failure);
}

TEST_F(GameplaySpecTest, ShootAppliesDamageAndBroadcastsHit)
{
    setUpRoom();
    setUpOpposingTeams();
    auto targetMessages = receivedMessages(*m_target);

    ASSERT_EQ(read(shootBytes(TARGET_ID, 25.0f), *m_sender), server::CommandResult::success);

    auto* room = server::RoomStorage::getRoomById(ROOM_ID);
    EXPECT_FLOAT_EQ(room->getPlayerHealth(TARGET_ID), DEFAULT_HEALTH - 25.0f);

    // A non-lethal hit never records kills/deaths or invokes the death path.
    EXPECT_EQ(room->getPlayerKills(SENDER_ID), 0);
    EXPECT_EQ(room->getPlayerDeaths(TARGET_ID), 0);

    ASSERT_EQ(targetMessages.size(), 1);
    const auto& msg = targetMessages[0];
    EXPECT_EQ(msg.at("command").as_string(), "room");
    EXPECT_EQ(msg.at("type").as_string(), "player_3D");
    EXPECT_EQ(msg.at("action").as_string(), "shoot");
    EXPECT_EQ(Util::toUint64(msg.at("room_id")), ROOM_ID);
    EXPECT_EQ(Util::toUint64(msg.at("client_id")), SENDER_ID);
    EXPECT_EQ(Util::toUint64(msg.at("target_id")), TARGET_ID);
    EXPECT_DOUBLE_EQ(msg.at("damage").as_double(), 25.0);
    EXPECT_DOUBLE_EQ(msg.at("new_health").as_double(), DEFAULT_HEALTH - 25.0);
    EXPECT_EQ(Util::toUint64(msg.at("callback")), MESSAGE_ID);
}

TEST_F(GameplaySpecTest, ShootKillTriggersDeathPathAndRecordsStats)
{
    setUpRoom();
    setUpOpposingTeams();
    auto targetMessages = receivedMessages(*m_target);

    // The application's death handler is responsible for recording the kill
    // stats (the shoot command only raises the event once the hit is lethal).
    std::vector<std::pair<uint64_t, uint64_t>> deaths;
    server::RoomStorage::getRoomById(ROOM_ID)->setDeathHandler(
            [&deaths](server::Room& room, uint64_t killerId, uint64_t victimId)
            {
                room.recordKill(killerId, victimId);
                deaths.emplace_back(killerId, victimId);
            });

    // A single shot is capped at 35 damage, so bring the target down to the
    // health one legal shot can take away first.
    setTargetHealth(30.0f);

    ASSERT_EQ(read(shootBytes(TARGET_ID, 30.0f), *m_sender), server::CommandResult::success);

    auto* room = server::RoomStorage::getRoomById(ROOM_ID);
    EXPECT_FLOAT_EQ(room->getPlayerHealth(TARGET_ID), 0.0f);
    EXPECT_EQ(room->getPlayerKills(SENDER_ID), 1);
    EXPECT_EQ(room->getPlayerDeaths(TARGET_ID), 1);

    ASSERT_EQ(deaths.size(), 1);
    EXPECT_EQ(deaths[0].first, SENDER_ID);
    EXPECT_EQ(deaths[0].second, TARGET_ID);

    // The hit broadcast reports the target at zero health.
    ASSERT_EQ(targetMessages.size(), 1);
    EXPECT_DOUBLE_EQ(targetMessages[0].at("new_health").as_double(), 0.0);
}

TEST_F(GameplaySpecTest, ShootOnDeadTargetDoesNotRecordSecondKill)
{
    setUpRoom();
    setUpOpposingTeams();

    std::vector<std::pair<uint64_t, uint64_t>> deaths;
    server::RoomStorage::getRoomById(ROOM_ID)->setDeathHandler(
            [&deaths](server::Room& room, uint64_t killerId, uint64_t victimId)
            {
                room.recordKill(killerId, victimId);
                deaths.emplace_back(killerId, victimId);
            });

    setTargetHealth(30.0f);

    ASSERT_EQ(read(shootBytes(TARGET_ID, 30.0f), *m_sender), server::CommandResult::success);
    // Second shot at the already-dead (not yet respawned) target is a no-op.
    ASSERT_EQ(read(shootBytes(TARGET_ID, 30.0f), *m_sender), server::CommandResult::success);

    auto* room = server::RoomStorage::getRoomById(ROOM_ID);
    EXPECT_FLOAT_EQ(room->getPlayerHealth(TARGET_ID), 0.0f);
    EXPECT_EQ(room->getPlayerKills(SENDER_ID), 1);
    EXPECT_EQ(room->getPlayerDeaths(TARGET_ID), 1);
    EXPECT_EQ(deaths.size(), 1);
}

// ---------------------------------------------------------------------------
// Shoot validation (teams, damage cap)
// ---------------------------------------------------------------------------

TEST_F(GameplaySpecTest, ShootRequiresOpposingTeams)
{
    setUpRoom();

    // Nobody has a team yet, so the shot is rejected.
    EXPECT_EQ(read(shootBytes(TARGET_ID, 25.0f), *m_sender), server::CommandResult::invalid_input);

    auto* room = server::RoomStorage::getRoomById(ROOM_ID);
    room->setPlayerTeam(SENDER_ID, "Terrorist");
    room->setPlayerTeam(TARGET_ID, "Terrorist");

    // Teammates can not damage each other either.
    EXPECT_EQ(read(shootBytes(TARGET_ID, 25.0f), *m_sender), server::CommandResult::invalid_input);
    EXPECT_FLOAT_EQ(room->getPlayerHealth(TARGET_ID), DEFAULT_HEALTH);

    room->setPlayerTeam(TARGET_ID, "Counter Terrorist");

    ASSERT_EQ(read(shootBytes(TARGET_ID, 25.0f), *m_sender), server::CommandResult::success);
    EXPECT_FLOAT_EQ(room->getPlayerHealth(TARGET_ID), DEFAULT_HEALTH - 25.0f);
}

TEST_F(GameplaySpecTest, ShootClampsDamageAbovePerShotCap)
{
    setUpRoom();
    setUpOpposingTeams();

    ASSERT_EQ(read(shootBytes(TARGET_ID, 35.1f), *m_sender), server::CommandResult::success);

    auto* room = server::RoomStorage::getRoomById(ROOM_ID);
    EXPECT_FLOAT_EQ(room->getPlayerHealth(TARGET_ID), DEFAULT_HEALTH - 35.0f);
}

TEST_F(GameplaySpecTest, ShootRejectsSelfTargetingAndNonPositiveDamage)
{
    setUpRoom();
    setUpOpposingTeams();

    EXPECT_EQ(read(shootBytes(SENDER_ID, 25.0f), *m_sender), server::CommandResult::invalid_input);
    EXPECT_EQ(read(shootBytes(TARGET_ID, 0.0f), *m_sender), server::CommandResult::invalid_input);

    auto* room = server::RoomStorage::getRoomById(ROOM_ID);
    EXPECT_FLOAT_EQ(room->getPlayerHealth(TARGET_ID), DEFAULT_HEALTH);
}

// ---------------------------------------------------------------------------
// Gameplay limits (position, movement speed, shooting rate, damage, height)
// ---------------------------------------------------------------------------

TEST(GameplayLimitsDefaultsTest, MatchesDocumentedDefaults)
{
    const server::GameplayLimits limits;

    EXPECT_FLOAT_EQ(limits.getMaxHorizontalPosition(), nexilis::NEXILIS_MAX_POSITION);
    EXPECT_FLOAT_EQ(limits.getMaxMovementSpeed(), 100.0f);
    EXPECT_FLOAT_EQ(limits.getMaxShootingRate(), 10.0f);
    EXPECT_FLOAT_EQ(limits.getMaxDamagePerShot(), 35.0f);
    EXPECT_FLOAT_EQ(limits.getMinHeight(), 0.0f);
    EXPECT_FLOAT_EQ(limits.getMaxHeight(), 1000.0f);
}

TEST(GameplayLimitsClampTest, ClampsValuesIntoConfiguredRanges)
{
    server::GameplayLimits limits;
    limits.setMaxHorizontalPosition(50.0f);
    limits.setMinHeight(-10.0f);
    limits.setMaxHeight(20.0f);
    limits.setMaxMovementSpeed(5.0f);
    limits.setMaxDamagePerShot(7.0f);

    EXPECT_FLOAT_EQ(limits.clampHorizontal(100.0f), 50.0f);
    EXPECT_FLOAT_EQ(limits.clampHorizontal(-100.0f), -50.0f);
    EXPECT_FLOAT_EQ(limits.clampHorizontal(10.0f), 10.0f);
    EXPECT_FLOAT_EQ(limits.clampHeight(100.0f), 20.0f);
    EXPECT_FLOAT_EQ(limits.clampHeight(-100.0f), -10.0f);
    EXPECT_FLOAT_EQ(limits.clampSpeed(100.0f), 5.0f);
    EXPECT_FLOAT_EQ(limits.clampDamage(100.0f), 7.0f);
    EXPECT_FLOAT_EQ(limits.shotInterval().count(), 0.1f);
}

TEST_F(GameplaySpecTest, PositionClampsHorizontalAndHeightToLimits)
{
    setUpRoom();
    auto targetMessages = receivedMessages(*m_target);

    const Vector3f position(20000.0f, -5.0f, 20000.0f);
    ASSERT_EQ(read(player3DPositionBytes(position), *m_sender), server::CommandResult::success);

    auto serverPosition = m_sender->getObject3D().getPosition();
    EXPECT_FLOAT_EQ(serverPosition.x, nexilis::NEXILIS_MAX_POSITION);
    EXPECT_FLOAT_EQ(serverPosition.y, 0.0f);
    EXPECT_FLOAT_EQ(serverPosition.z, nexilis::NEXILIS_MAX_POSITION);

    // The broadcast carries the clamped position.
    ASSERT_EQ(targetMessages.size(), 1);
    EXPECT_DOUBLE_EQ(targetMessages[0].at("x").as_double(), nexilis::NEXILIS_MAX_POSITION);
    EXPECT_DOUBLE_EQ(targetMessages[0].at("y").as_double(), 0.0);
    EXPECT_DOUBLE_EQ(targetMessages[0].at("z").as_double(), nexilis::NEXILIS_MAX_POSITION);
}

TEST_F(GameplaySpecTest, PositionClampsToConfiguredLimits)
{
    setUpRoom();
    command.getServerConfig().getGameplayLimits().setMaxHorizontalPosition(50.0f);
    command.getServerConfig().getGameplayLimits().setMinHeight(-10.0f);
    command.getServerConfig().getGameplayLimits().setMaxHeight(20.0f);

    ASSERT_EQ(read(player3DPositionBytes({100.0f, 100.0f, -100.0f}), *m_sender),
              server::CommandResult::success);

    auto serverPosition = m_sender->getObject3D().getPosition();
    EXPECT_FLOAT_EQ(serverPosition.x, 50.0f);
    EXPECT_FLOAT_EQ(serverPosition.y, 20.0f);
    EXPECT_FLOAT_EQ(serverPosition.z, -50.0f);
}

TEST_F(GameplaySpecTest, MovementClampsSpeedToLimit)
{
    setUpRoom();

    // 300 units/s exceeds the default 100 units/s cap, so the
    // displacement is capped at 100 * 0.1 = 10.
    ASSERT_EQ(read(player3DMovementBytes({300.0f, 0.0f, 0.0f}, 0.1f), *m_sender),
              server::CommandResult::success);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto serverPosition = m_sender->getObject3D().getPosition();
    EXPECT_NEAR(serverPosition.x, 10.0f, 0.01f);
    EXPECT_FLOAT_EQ(serverPosition.y, 0.0f);
    EXPECT_FLOAT_EQ(serverPosition.z, 0.0f);
}

TEST_F(GameplaySpecTest, MovementClampsResultingPositionToLimits)
{
    setUpRoom();
    command.getServerConfig().getGameplayLimits().setMaxHorizontalPosition(100.0f);

    m_sender->getObject3D().setPosition({90.0f, 500.0f, 0.0f});

    // One second at 1000 units/s would reach x = 1090; both the
    // speed and the resulting position are clamped.
    ASSERT_EQ(read(player3DMovementBytes({1000.0f, 0.0f, 0.0f}, 1.0f), *m_sender),
              server::CommandResult::success);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto serverPosition = m_sender->getObject3D().getPosition();
    EXPECT_NEAR(serverPosition.x, 100.0f, 0.01f);
}

TEST_F(GameplaySpecTest, ShootRateLimitDropsFastFollowUpShot)
{
    setUpRoom();
    setUpOpposingTeams();
    command.getServerConfig().getGameplayLimits().setMaxShootingRate(1.0f);

    ASSERT_EQ(read(shootBytes(TARGET_ID, 25.0f), *m_sender), server::CommandResult::success);
    // The follow-up shot arrives inside the one second interval
    // and is dropped.
    EXPECT_EQ(read(shootBytes(TARGET_ID, 25.0f), *m_sender), server::CommandResult::invalid_input);

    auto* room = server::RoomStorage::getRoomById(ROOM_ID);
    EXPECT_FLOAT_EQ(room->getPlayerHealth(TARGET_ID), DEFAULT_HEALTH - 25.0f);
}

TEST_F(GameplaySpecTest, ShootRateLimitAllowsShotAfterInterval)
{
    setUpRoom();
    setUpOpposingTeams();
    command.getServerConfig().getGameplayLimits().setMaxShootingRate(10.0f);

    ASSERT_EQ(read(shootBytes(TARGET_ID, 25.0f), *m_sender), server::CommandResult::success);
    std::this_thread::sleep_for(std::chrono::milliseconds(110));
    ASSERT_EQ(read(shootBytes(TARGET_ID, 25.0f), *m_sender), server::CommandResult::success);

    auto* room = server::RoomStorage::getRoomById(ROOM_ID);
    EXPECT_FLOAT_EQ(room->getPlayerHealth(TARGET_ID), DEFAULT_HEALTH - 50.0f);
}

// ---------------------------------------------------------------------------
// Team
// ---------------------------------------------------------------------------

TEST_F(GameplaySpecTest, SetTeamNotInRoomFails)
{
    EXPECT_EQ(read(setTeamBytes("Terrorist"), *m_sender), server::CommandResult::error);
}

TEST_F(GameplaySpecTest, SetTeamUnknownTeamFails)
{
    setUpRoom();
    EXPECT_EQ(read(setTeamBytes("Blue"), *m_sender), server::CommandResult::invalid_input);
}

TEST_F(GameplaySpecTest, SetTeamRoomNotFoundFails)
{
    m_sender->setRoomId(ROOM_ID);
    EXPECT_EQ(read(setTeamBytes("Terrorist"), *m_sender), server::CommandResult::failure);
}

TEST_F(GameplaySpecTest, SetTeamValidatesTeamValueAndStoresIt)
{
    setUpRoom();
    auto* room = server::RoomStorage::getRoomById(ROOM_ID);

    ASSERT_EQ(read(setTeamBytes("Terrorist"), *m_sender), server::CommandResult::success);
    EXPECT_EQ(room->getPlayerTeam(SENDER_ID), "Terrorist");

    ASSERT_EQ(read(setTeamBytes("Counter Terrorist"), *m_sender), server::CommandResult::success);
    EXPECT_EQ(room->getPlayerTeam(SENDER_ID), "Counter Terrorist");
}

TEST_F(GameplaySpecTest, SetTeamAnnouncesLeaderboardToRoomAndSeedsJoiner)
{
    setUpRoom();
    m_sender->setUsername("shooters");

    auto senderMessages = receivedMessages(*m_sender);
    auto targetMessages = receivedMessages(*m_target);

    ASSERT_EQ(read(setTeamBytes("Terrorist"), *m_sender), server::CommandResult::success);

    auto* room = server::RoomStorage::getRoomById(ROOM_ID);
    EXPECT_EQ(room->getPlayerTeam(SENDER_ID), "Terrorist");

    // The joining player receives the room broadcast plus the seed of every
    // member's stats (the room broadcast payload first).
    ASSERT_EQ(senderMessages.size(), 2);

    const auto& broadcast = senderMessages[0];
    EXPECT_EQ(broadcast.at("command").as_string(), "room");
    EXPECT_EQ(broadcast.at("type").as_string(), "player_3D");
    EXPECT_EQ(broadcast.at("action").as_string(), "leaderboard");
    EXPECT_EQ(Util::toUint64(broadcast.at("callback")), 0);
    const auto& broadcastEntries = broadcast.at("entries").as_array();
    ASSERT_EQ(broadcastEntries.size(), 1);
    EXPECT_EQ(Util::toUint64(broadcastEntries[0].at("id")), SENDER_ID);
    EXPECT_EQ(broadcastEntries[0].at("username").as_string(), "shooters");
    EXPECT_EQ(broadcastEntries[0].at("team").as_string(), "Terrorist");
    EXPECT_EQ(Util::toUint64(broadcastEntries[0].at("kills")), 0);
    EXPECT_EQ(Util::toUint64(broadcastEntries[0].at("deaths")), 0);

    // The seed carries the joining player's entry and the other member's.
    const auto& seed = senderMessages[1];
    EXPECT_EQ(seed.at("action").as_string(), "leaderboard");
    EXPECT_EQ(Util::toUint64(seed.at("callback")), MESSAGE_ID);
    const auto& seedEntries = seed.at("entries").as_array();
    ASSERT_EQ(seedEntries.size(), 2);
    EXPECT_EQ(Util::toUint64(seedEntries[0].at("id")), SENDER_ID);
    EXPECT_EQ(seedEntries[0].at("team").as_string(), "Terrorist");
    EXPECT_EQ(Util::toUint64(seedEntries[1].at("id")), TARGET_ID);
    EXPECT_EQ(seedEntries[1].at("team").as_string(), "");

    // The other member only receives the broadcast of the joining player's stats.
    ASSERT_EQ(targetMessages.size(), 1);
    EXPECT_EQ(targetMessages[0].at("action").as_string(), "leaderboard");
    EXPECT_EQ(targetMessages[0].at("entries").as_array().size(), 1);
}

// ---------------------------------------------------------------------------
// Leaderboard helpers (server side)
// ---------------------------------------------------------------------------

TEST_F(GameplaySpecTest, PlayerStatsEntriesBuildsFullRows)
{
    setUpRoom();
    m_sender->setUsername("shooters");
    m_target->setUsername("victims");

    auto* room = server::RoomStorage::getRoomById(ROOM_ID);
    room->setPlayerTeam(SENDER_ID, "Terrorist");
    room->recordKill(SENDER_ID, TARGET_ID);
    room->recordKill(SENDER_ID, TARGET_ID);

    auto entries = server::Command::playerStatsEntries(*room, {SENDER_ID, TARGET_ID});
    ASSERT_EQ(entries.size(), 2);

    const auto& killer = entries[0].as_object();
    EXPECT_EQ(Util::toUint64(killer.at("id")), SENDER_ID);
    EXPECT_EQ(killer.at("username").as_string(), "shooters");
    EXPECT_EQ(killer.at("team").as_string(), "Terrorist");
    EXPECT_EQ(Util::toUint64(killer.at("kills")), 2);
    EXPECT_EQ(Util::toUint64(killer.at("deaths")), 0);

    const auto& victim = entries[1].as_object();
    EXPECT_EQ(Util::toUint64(victim.at("id")), TARGET_ID);
    EXPECT_EQ(victim.at("username").as_string(), "victims");
    EXPECT_EQ(victim.at("team").as_string(), "");
    EXPECT_EQ(Util::toUint64(victim.at("kills")), 0);
    EXPECT_EQ(Util::toUint64(victim.at("deaths")), 2);
}

TEST_F(GameplaySpecTest, CreateRoomLeaderboardCommandWrapsEntries)
{
    setUpRoom();

    boost::json::array entries;
    boost::json::object row;
    row["id"] = TARGET_ID;
    row["username"] = "victims";
    row["team"] = "Terrorist";
    row["kills"] = 1;
    row["deaths"] = 3;
    entries.emplace_back(std::move(row));

    auto payload = server::Command::createRoomLeaderboardCommand(ROOM_ID, *m_sender, entries, MESSAGE_ID);
    const auto json = parseMessage(payload);

    EXPECT_EQ(json.at("command").as_string(), "room");
    EXPECT_EQ(json.at("type").as_string(), "player_3D");
    EXPECT_EQ(json.at("action").as_string(), "leaderboard");
    EXPECT_EQ(Util::toUint64(json.at("callback")), MESSAGE_ID);
    EXPECT_EQ(Util::toUint64(json.at("room_id")), ROOM_ID);
    EXPECT_EQ(Util::toUint64(json.at("client_id")), SENDER_ID);

    const auto& wireEntries = json.at("entries").as_array();
    ASSERT_EQ(wireEntries.size(), 1);
    EXPECT_EQ(Util::toUint64(wireEntries[0].at("id")), TARGET_ID);
    EXPECT_EQ(wireEntries[0].at("team").as_string(), "Terrorist");
}

// ---------------------------------------------------------------------------
// Kill -> respawn / leaderboard flows
// ---------------------------------------------------------------------------

TEST_F(GameplaySpecTest, RespawnFlowAllowsTargetToBeDamagedAgain)
{
    setUpRoom();
    setUpOpposingTeams();

    // Application respawn policy: a killed player is revived at full health
    // (the shoot command itself only raises the death event).
    server::RoomStorage::getRoomById(ROOM_ID)->setDeathHandler(
            [](server::Room& room, uint64_t killerId, uint64_t victimId)
            {
                room.recordKill(killerId, victimId);
                room.resetPlayerHealth(victimId);
            });

    // One shot is capped at 35 damage, so the target has to be weakened first
    // for a single shot to be lethal.
    setTargetHealth(30.0f);

    ASSERT_EQ(read(shootBytes(TARGET_ID, 30.0f), *m_sender), server::CommandResult::success);

    auto* room = server::RoomStorage::getRoomById(ROOM_ID);
    // The handler respawned the victim immediately, so health is back to full.
    EXPECT_FLOAT_EQ(room->getPlayerHealth(TARGET_ID), DEFAULT_HEALTH);
    EXPECT_EQ(room->getPlayerKills(SENDER_ID), 1);
    EXPECT_EQ(room->getPlayerDeaths(TARGET_ID), 1);

    // A follow-up shot now wounds the respawned target normally.
    ASSERT_EQ(read(shootBytes(TARGET_ID, 30.0f), *m_sender), server::CommandResult::success);
    EXPECT_FLOAT_EQ(room->getPlayerHealth(TARGET_ID), DEFAULT_HEALTH - 30.0f);
    EXPECT_EQ(room->getPlayerKills(SENDER_ID), 1);
    EXPECT_EQ(room->getPlayerDeaths(TARGET_ID), 1);
}

TEST_F(GameplaySpecTest, KillBroadcastsLeaderboardDeltaAndRespawnAction)
{
    setUpRoom();
    setUpOpposingTeams();
    m_sender->setUsername("shooters");
    m_target->setUsername("victims");

    auto senderMessages = receivedMessages(*m_sender);
    auto targetMessages = receivedMessages(*m_target);

    // The application death handler records the kill, announces the killer +
    // victim leaderboard delta to the room, and tells everyone where the victim
    // respawns (exercising the leaderboard helpers end to end).
    server::RoomStorage::getRoomById(ROOM_ID)->setDeathHandler(
            [this](server::Room& room, uint64_t killerId, uint64_t victimId)
            {
                room.recordKill(killerId, victimId);

                auto entries = server::Command::playerStatsEntries(room, {killerId, victimId});
                auto leaderboard = server::Command::createRoomLeaderboardCommand(
                        room.getId(), *m_sender, std::move(entries), 0);
                room.broadcastToAll(leaderboard);

                nx_data respawnData{static_cast<uint8_t>(CommandType::room),
                                    static_cast<uint8_t>(RoomCommandType::Root::player_3D),
                                    static_cast<uint8_t>(RoomCommandType::PlayerType::respawn)};
                std::map<std::string, boost::json::value> respawnParams{
                        {"target_id", boost::json::value(victimId)},
                        {"x", boost::json::value(0.0)},
                        {"y", boost::json::value(0.0)},
                        {"z", boost::json::value(0.0)}};
                room.broadcastToAll(server::Command::createRoomCommand(
                        room.getId(), *m_sender, respawnData, respawnParams, 0));
            });

    // One shot is capped at 35 damage, so weaken the target to a single legal
    // shot's worth of health before the killing blow.
    setTargetHealth(30.0f);

    ASSERT_EQ(read(shootBytes(TARGET_ID, 30.0f), *m_sender), server::CommandResult::success);

    // Every room member receives the hit broadcast, the leaderboard delta and
    // the respawn announcement.
    auto checkMessages = [](const std::vector<boost::json::object>& messages)
    {
        ASSERT_EQ(messages.size(), 3);
        EXPECT_EQ(messages[0].at("action").as_string(), "shoot");

        const auto& board = messages[1];
        EXPECT_EQ(board.at("action").as_string(), "leaderboard");
        const auto& boardEntries = board.at("entries").as_array();
        ASSERT_EQ(boardEntries.size(), 2);
        EXPECT_EQ(Util::toUint64(boardEntries[0].at("id")), SENDER_ID);
        EXPECT_EQ(Util::toUint64(boardEntries[0].at("kills")), 1);
        EXPECT_EQ(Util::toUint64(boardEntries[0].at("deaths")), 0);
        EXPECT_EQ(Util::toUint64(boardEntries[1].at("id")), TARGET_ID);
        EXPECT_EQ(Util::toUint64(boardEntries[1].at("kills")), 0);
        EXPECT_EQ(Util::toUint64(boardEntries[1].at("deaths")), 1);

        const auto& respawn = messages[2];
        EXPECT_EQ(respawn.at("action").as_string(), "respawn");
        EXPECT_EQ(Util::toUint64(respawn.at("target_id")), TARGET_ID);
    };
    checkMessages(senderMessages);
    checkMessages(targetMessages);
}

// ---------------------------------------------------------------------------
// Room state primitives
// ---------------------------------------------------------------------------

TEST(RoomStateTest, DamageClampsToZeroAndIgnoresDeadTarget)
{
    server::Room room(RoomData(1, "state_room", ROOM_ID));

    EXPECT_FALSE(room.damagePlayer(TARGET_ID, 30.0f));
    EXPECT_FLOAT_EQ(room.getPlayerHealth(TARGET_ID), 70.0f);

    // This hit brings the target from alive to dead.
    EXPECT_TRUE(room.damagePlayer(TARGET_ID, 70.0f));
    EXPECT_FLOAT_EQ(room.getPlayerHealth(TARGET_ID), 0.0f);

    // Dead players absorb no further damage and never re-trigger a death.
    EXPECT_FALSE(room.damagePlayer(TARGET_ID, 50.0f));
    EXPECT_FLOAT_EQ(room.getPlayerHealth(TARGET_ID), 0.0f);
}

TEST(RoomStateTest, ResetHealthRestoresDefault)
{
    server::Room room(RoomData(1, "state_room", ROOM_ID));

    ASSERT_TRUE(room.damagePlayer(TARGET_ID, DEFAULT_HEALTH));
    EXPECT_FLOAT_EQ(room.getPlayerHealth(TARGET_ID), 0.0f);

    room.resetPlayerHealth(TARGET_ID);
    EXPECT_FLOAT_EQ(room.getPlayerHealth(TARGET_ID), DEFAULT_HEALTH);
}

TEST(RoomStateTest, RecordKillUpdatesKillerAndVictimStats)
{
    server::Room room(RoomData(1, "state_room", ROOM_ID));

    room.recordKill(SENDER_ID, TARGET_ID);
    room.recordKill(SENDER_ID, TARGET_ID);

    EXPECT_EQ(room.getPlayerKills(SENDER_ID), 2);
    EXPECT_EQ(room.getPlayerDeaths(TARGET_ID), 2);
    EXPECT_EQ(room.getPlayerKills(TARGET_ID), 0);
    EXPECT_EQ(room.getPlayerDeaths(SENDER_ID), 0);
}

TEST(RoomStateTest, TeamsAreEmptyByDefaultAndStoredOnSet)
{
    server::Room room(RoomData(1, "state_room", ROOM_ID));

    EXPECT_EQ(room.getPlayerTeam(SENDER_ID), "");

    room.setPlayerTeam(SENDER_ID, "Counter Terrorist");
    EXPECT_EQ(room.getPlayerTeam(SENDER_ID), "Counter Terrorist");
}

TEST(RoomStateTest, DeathHandlerRunsOnPlayerDeath)
{
    server::Room room(RoomData(1, "state_room", ROOM_ID));

    std::vector<std::pair<uint64_t, uint64_t>> deaths;
    room.setDeathHandler([&deaths](server::Room&, uint64_t killerId, uint64_t victimId)
                         { deaths.emplace_back(killerId, victimId); });

    room.onPlayerDied(SENDER_ID, TARGET_ID);
    ASSERT_EQ(deaths.size(), 1);
    EXPECT_EQ(deaths[0].first, SENDER_ID);
    EXPECT_EQ(deaths[0].second, TARGET_ID);
}

// ---------------------------------------------------------------------------
// Moving objects (server side)
// ---------------------------------------------------------------------------

TEST_F(GameplaySpecTest, CreateMovingObject2DAddsObjectAndBroadcastsCreate)
{
    setUpRoom();
    auto targetMessages = receivedMessages(*m_target);

    const Vector2f start(10.0f, 20.0f);
    const Vector2f dims(2.0f, 3.0f);
    auto bytes = createMoving2DBytes(start, dims, Vector2f(1.0f, 0.0f), 0.001f,
                                     MovementType::eased, "sprites/ball.png");
    ASSERT_EQ(read(bytes, *m_sender), server::CommandResult::success);

    auto* room = server::RoomStorage::getRoomById(ROOM_ID);
    ASSERT_EQ(room->getObjects2D().size(), 1);
    const auto& object = room->getObjects2D()[0];
    EXPECT_FLOAT_EQ(object.getPosition().x, start.x);
    EXPECT_FLOAT_EQ(object.getPosition().y, start.y);
    EXPECT_FLOAT_EQ(object.getDimensions().x, dims.x);
    EXPECT_FLOAT_EQ(object.getDimensions().y, dims.y);
    EXPECT_EQ(object.getFilepath(), "sprites/ball.png");

    ASSERT_EQ(targetMessages.size(), 1);
    const auto& msg = targetMessages[0];
    EXPECT_EQ(msg.at("command").as_string(), "room");
    EXPECT_EQ(msg.at("type").as_string(), "object_2D");
    EXPECT_EQ(msg.at("action").as_string(), "create_moving");
    EXPECT_EQ(msg.at("createMovingType").as_string(), "create");
    EXPECT_EQ(Util::toUint64(msg.at("id")), object.getId());
    EXPECT_DOUBLE_EQ(msg.at("x").as_double(), start.x);
    EXPECT_DOUBLE_EQ(msg.at("y").as_double(), start.y);
    EXPECT_DOUBLE_EQ(msg.at("width").as_double(), dims.x);
    EXPECT_DOUBLE_EQ(msg.at("height").as_double(), dims.y);
    EXPECT_EQ(msg.at("filepath").as_string(), "sprites/ball.png");
    EXPECT_EQ(Util::toUint64(msg.at("room_id")), ROOM_ID);
    EXPECT_EQ(Util::toUint64(msg.at("client_id")), SENDER_ID);
    EXPECT_EQ(Util::toUint64(msg.at("callback")), MESSAGE_ID);

    // Give the (immediately terminating) movement thread a moment to exit
    // before TearDown releases the shared storage it reads.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(GameplaySpecTest, CreateMovingObject3DAddsObjectAndBroadcastsCreate)
{
    setUpRoom();
    auto targetMessages = receivedMessages(*m_target);

    const Vector3f start(1.0f, 2.0f, 3.0f);
    const Vector3f dims(4.0f, 5.0f, 6.0f);
    auto bytes = createMoving3DBytes(start, dims, Vector3f(1.0f, 0.0f, 0.0f), 0.001f,
                                     MovementType::linear, "models/crate.obj");
    ASSERT_EQ(read(bytes, *m_sender), server::CommandResult::success);

    auto* room = server::RoomStorage::getRoomById(ROOM_ID);
    ASSERT_EQ(room->getObjects3D().size(), 1);
    const auto& object = room->getObjects3D()[0];
    EXPECT_FLOAT_EQ(object.getPosition().x, start.x);
    EXPECT_FLOAT_EQ(object.getPosition().y, start.y);
    EXPECT_FLOAT_EQ(object.getPosition().z, start.z);
    EXPECT_FLOAT_EQ(object.getDimensions().x, dims.x);
    EXPECT_FLOAT_EQ(object.getDimensions().y, dims.y);
    EXPECT_FLOAT_EQ(object.getDimensions().z, dims.z);
    EXPECT_EQ(object.getFilepath(), "models/crate.obj");

    ASSERT_EQ(targetMessages.size(), 1);
    const auto& msg = targetMessages[0];
    EXPECT_EQ(msg.at("type").as_string(), "object_3D");
    EXPECT_EQ(msg.at("action").as_string(), "create_moving");
    EXPECT_EQ(msg.at("createMovingType").as_string(), "create");
    EXPECT_EQ(Util::toUint64(msg.at("id")), object.getId());
    EXPECT_DOUBLE_EQ(msg.at("x").as_double(), start.x);
    EXPECT_DOUBLE_EQ(msg.at("y").as_double(), start.y);
    EXPECT_DOUBLE_EQ(msg.at("z").as_double(), start.z);
    EXPECT_DOUBLE_EQ(msg.at("w").as_double(), dims.x);
    EXPECT_DOUBLE_EQ(msg.at("h").as_double(), dims.y);
    EXPECT_DOUBLE_EQ(msg.at("d").as_double(), dims.z);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(GameplaySpecTest, CreateMovingLinearObjectMovesOverTime)
{
    setUpRoom();
    auto targetMessages = receivedMessages(*m_target);

    const float delta = 0.25f; // 15 ticks at the default 60 Hz tickrate.
    auto bytes = createMoving2DBytes(Vector2f(0.0f, 0.0f), Vector2f(1.0f, 1.0f),
                                     Vector2f(10.0f, 0.0f), delta, MovementType::linear, "");
    ASSERT_EQ(read(bytes, *m_sender), server::CommandResult::success);

    // Wait until the movement thread has run to completion and detached.
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    auto* room = server::RoomStorage::getRoomById(ROOM_ID);
    ASSERT_EQ(room->getObjects2D().size(), 1);
    auto position = room->getObjects2D()[0].getPosition();

    // The object advanced along the x axis without overshooting its target.
    EXPECT_GT(position.x, 0.0f);
    EXPECT_LE(position.x, 10.0f + 0.01f);
    EXPECT_FLOAT_EQ(position.y, 0.0f);

    // The server streamed at least one update to the room members.
    bool foundUpdate = false;
    for (const auto& msg : targetMessages)
    {
        if (msg.at("createMovingType").as_string() == "update")
        {
            foundUpdate = true;
            EXPECT_TRUE(msg.contains("id"));
            EXPECT_GT(msg.at("x").as_double(), 0.0);
            break;
        }
    }
    EXPECT_TRUE(foundUpdate);
}

// ---------------------------------------------------------------------------
// Player & object commands (server side)
// ---------------------------------------------------------------------------

TEST_F(GameplaySpecTest, Player3DPositionNotInRoomFails)
{
    // The player has not joined any room (room id stays 0).
    EXPECT_EQ(read(player3DPositionBytes({1.0f, 2.0f, 3.0f}), *m_sender), server::CommandResult::error);
}

TEST_F(GameplaySpecTest, Player3DPositionUpdatesServerAndBroadcasts)
{
    setUpRoom();
    auto targetMessages = receivedMessages(*m_target);

    const Vector3f position(10.0f, 20.0f, 30.0f);
    ASSERT_EQ(read(player3DPositionBytes(position), *m_sender), server::CommandResult::success);

    auto serverPosition = m_sender->getObject3D().getPosition();
    EXPECT_FLOAT_EQ(serverPosition.x, position.x);
    EXPECT_FLOAT_EQ(serverPosition.y, position.y);
    EXPECT_FLOAT_EQ(serverPosition.z, position.z);

    ASSERT_EQ(targetMessages.size(), 1);
    const auto& msg = targetMessages[0];
    EXPECT_EQ(msg.at("action").as_string(), "position");
    EXPECT_EQ(Util::toUint64(msg.at("room_id")), ROOM_ID);
    EXPECT_EQ(Util::toUint64(msg.at("client_id")), SENDER_ID);
    EXPECT_EQ(Util::toUint64(msg.at("callback")), MESSAGE_ID);
    EXPECT_DOUBLE_EQ(msg.at("x").as_double(), position.x);
    EXPECT_DOUBLE_EQ(msg.at("y").as_double(), position.y);
    EXPECT_DOUBLE_EQ(msg.at("z").as_double(), position.z);
}

TEST_F(GameplaySpecTest, Player3DPositionWithYawUpdatesServerAndBroadcasts)
{
    setUpRoom();
    auto targetMessages = receivedMessages(*m_target);

    auto bytes = player3DPositionBytes({10.0f, 20.0f, 30.0f});
    auto yawBytes = Util::convertToByteVector(1.25f);
    bytes.insert(bytes.end(), yawBytes.begin(), yawBytes.end());
    ASSERT_EQ(read(bytes, *m_sender), server::CommandResult::success);
    EXPECT_FLOAT_EQ(m_sender->getObject3D().getYaw(), 1.25f);
    ASSERT_EQ(targetMessages.size(), 1);
    EXPECT_DOUBLE_EQ(targetMessages[0].at("yaw").as_double(), 1.25);
}

TEST_F(GameplaySpecTest, JoiningPlayerReceivesExistingPlayerYaw)
{
    server::RoomStorage::add(server::Room(RoomData(SENDER_ID, "gameplay_room", ROOM_ID)));
    auto* room = server::RoomStorage::getRoomById(ROOM_ID);
    room->joinRoom(SENDER_ID);
    m_sender->setRoomId(ROOM_ID);
    m_sender->setBoostTCPSend([](const nx_data&) {});
    m_sender->getObject3D().setPosition({10.0f, 20.0f, 30.0f});
    m_sender->getObject3D().setYaw(1.25f);

    auto targetMessages = receivedMessages(*m_target);
    nx_data joinBytes{static_cast<uint8_t>(CommandType::room),
                      static_cast<uint8_t>(RoomCommandType::Root::management),
                      static_cast<uint8_t>(RoomCommandType::Management::join)};
    auto roomBytes = Util::convertToByteVector(ROOM_ID);
    joinBytes.insert(joinBytes.end(), roomBytes.begin(), roomBytes.end());

    ASSERT_EQ(read(joinBytes, *m_target), server::CommandResult::success);
    auto position = std::find_if(targetMessages.begin(), targetMessages.end(),
                                 [](const auto& message)
                                 {
                                     return message.at("type").as_string() == "player_3D" &&
                                            message.at("action").as_string() == "position";
                                 });
    ASSERT_NE(position, targetMessages.end());
    EXPECT_EQ(Util::toUint64(position->at("client_id")), SENDER_ID);
    EXPECT_DOUBLE_EQ(position->at("yaw").as_double(), 1.25);
}

TEST_F(GameplaySpecTest, Player3DDimensionUpdatesServerAndBroadcasts)
{
    setUpRoom();
    auto targetMessages = receivedMessages(*m_target);

    const Vector3f dimensions(2.0f, 4.0f, 6.0f);
    ASSERT_EQ(read(player3DDimensionBytes(dimensions), *m_sender), server::CommandResult::success);

    auto serverDimensions = m_sender->getObject3D().getDimensions();
    EXPECT_FLOAT_EQ(serverDimensions.x, dimensions.x);
    EXPECT_FLOAT_EQ(serverDimensions.y, dimensions.y);
    EXPECT_FLOAT_EQ(serverDimensions.z, dimensions.z);

    ASSERT_EQ(targetMessages.size(), 1);
    const auto& msg = targetMessages[0];
    EXPECT_EQ(msg.at("action").as_string(), "dimension");
    EXPECT_EQ(Util::toUint64(msg.at("callback")), MESSAGE_ID);
    EXPECT_DOUBLE_EQ(msg.at("x").as_double(), dimensions.x);
    EXPECT_DOUBLE_EQ(msg.at("y").as_double(), dimensions.y);
    EXPECT_DOUBLE_EQ(msg.at("z").as_double(), dimensions.z);
}

TEST_F(GameplaySpecTest, Player3DMovementAppliesDisplacementAndBroadcasts)
{
    setUpRoom();
    auto targetMessages = receivedMessages(*m_target);

    // The movement command displaces the player once: position + velocity * delta.
    const Vector3f velocity(10.0f, 0.0f, 0.0f);
    ASSERT_EQ(read(player3DMovementBytes(velocity, 0.1f), *m_sender), server::CommandResult::success);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto serverPosition = m_sender->getObject3D().getPosition();
    EXPECT_NEAR(serverPosition.x, 10.0f * 0.1f, 0.01f);
    EXPECT_FLOAT_EQ(serverPosition.y, 0.0f);
    EXPECT_FLOAT_EQ(serverPosition.z, 0.0f);

    ASSERT_GE(targetMessages.size(), 1);
    const auto& msg = targetMessages[0];
    EXPECT_EQ(msg.at("action").as_string(), "movement");
    EXPECT_EQ(Util::toUint64(msg.at("callback")), MESSAGE_ID);
    EXPECT_NEAR(msg.at("x").as_double(), 10.0f * 0.1f, 0.01);
}

TEST_F(GameplaySpecTest, Player3DMovementWithYawUpdatesServerAndBroadcasts)
{
    setUpRoom();
    auto targetMessages = receivedMessages(*m_target);

    auto bytes = player3DMovementBytes({10.0f, 0.0f, 0.0f}, 0.1f);
    auto yawBytes = Util::convertToByteVector(-0.75f);
    bytes.insert(bytes.end(), yawBytes.begin(), yawBytes.end());
    ASSERT_EQ(read(bytes, *m_sender), server::CommandResult::success);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FLOAT_EQ(m_sender->getObject3D().getYaw(), -0.75f);
    ASSERT_EQ(targetMessages.size(), 1);
    EXPECT_DOUBLE_EQ(targetMessages[0].at("yaw").as_double(), -0.75);
}

TEST_F(GameplaySpecTest, Player2DPositionNotInRoomFails)
{
    EXPECT_EQ(read(player2DPositionBytes({1.0f, 2.0f}), *m_sender), server::CommandResult::error);
}

TEST_F(GameplaySpecTest, Player2DPositionUpdatesServerAndBroadcasts)
{
    setUpRoom();
    auto targetMessages = receivedMessages(*m_target);

    const Vector2f position(15.0f, 25.0f);
    ASSERT_EQ(read(player2DPositionBytes(position), *m_sender), server::CommandResult::success);

    auto serverPosition = m_sender->getObject2D().getPosition();
    EXPECT_FLOAT_EQ(serverPosition.x, position.x);
    EXPECT_FLOAT_EQ(serverPosition.y, position.y);

    ASSERT_EQ(targetMessages.size(), 1);
    const auto& msg = targetMessages[0];
    EXPECT_EQ(msg.at("action").as_string(), "position");
    EXPECT_EQ(Util::toUint64(msg.at("callback")), MESSAGE_ID);
    EXPECT_DOUBLE_EQ(msg.at("x").as_double(), position.x);
    EXPECT_DOUBLE_EQ(msg.at("y").as_double(), position.y);
}

TEST_F(GameplaySpecTest, Player2DDimensionUpdatesServerAndBroadcasts)
{
    setUpRoom();
    auto targetMessages = receivedMessages(*m_target);

    const Vector2f dimensions(3.0f, 5.0f);
    ASSERT_EQ(read(player2DDimensionBytes(dimensions), *m_sender), server::CommandResult::success);

    auto serverDimensions = m_sender->getObject2D().getDimensions();
    EXPECT_FLOAT_EQ(serverDimensions.x, dimensions.x);
    EXPECT_FLOAT_EQ(serverDimensions.y, dimensions.y);

    ASSERT_EQ(targetMessages.size(), 1);
    const auto& msg = targetMessages[0];
    EXPECT_EQ(msg.at("action").as_string(), "dimension");
    EXPECT_EQ(Util::toUint64(msg.at("callback")), MESSAGE_ID);
    EXPECT_DOUBLE_EQ(msg.at("x").as_double(), dimensions.x);
    EXPECT_DOUBLE_EQ(msg.at("y").as_double(), dimensions.y);
}

TEST_F(GameplaySpecTest, Player2DMovementStreamsPositionUpdates)
{
    setUpRoom();
    auto targetMessages = receivedMessages(*m_target);

    // A 50 ms movement span produces a few tick updates at 60 Hz.
    const Vector2f velocity(2.0f, 0.0f);
    ASSERT_EQ(read(player2DMovementBytes(velocity, 0.05f), *m_sender), server::CommandResult::success);

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // The final server position matches the target displacement.
    auto serverPosition = m_sender->getObject2D().getPosition();
    EXPECT_NEAR(serverPosition.x, 2.0f * 1.0f, 0.01f);
    EXPECT_FLOAT_EQ(serverPosition.y, 0.0f);

    // And the streamed broadcasts carry the updated positions.
    bool foundUpdate = false;
    for (const auto& msg : targetMessages)
    {
        if (msg.at("action").as_string() == "movement")
        {
            foundUpdate = true;
            EXPECT_GT(msg.at("x").as_double(), 0.0);
            break;
        }
    }
    EXPECT_TRUE(foundUpdate);
}

TEST_F(GameplaySpecTest, CreateObject2DAddsStaticObjectAndBroadcasts)
{
    setUpRoom();
    auto targetMessages = receivedMessages(*m_target);

    const Vector2f position(7.0f, 8.0f);
    const Vector2f dimensions(2.0f, 3.0f);
    ASSERT_EQ(read(createObject2DBytes(position, dimensions, "sprites/tree.png"), *m_sender),
              server::CommandResult::success);

    auto* room = server::RoomStorage::getRoomById(ROOM_ID);
    ASSERT_EQ(room->getObjects2D().size(), 1);
    const auto& object = room->getObjects2D()[0];
    EXPECT_FLOAT_EQ(object.getPosition().x, position.x);
    EXPECT_FLOAT_EQ(object.getPosition().y, position.y);
    EXPECT_FLOAT_EQ(object.getDimensions().x, dimensions.x);
    EXPECT_FLOAT_EQ(object.getDimensions().y, dimensions.y);
    EXPECT_EQ(object.getFilepath(), "sprites/tree.png");

    ASSERT_EQ(targetMessages.size(), 1);
    const auto& msg = targetMessages[0];
    EXPECT_EQ(msg.at("type").as_string(), "object_2D");
    EXPECT_EQ(msg.at("action").as_string(), "create");
    EXPECT_EQ(Util::toUint64(msg.at("id")), object.getId());
    EXPECT_EQ(Util::toUint64(msg.at("callback")), MESSAGE_ID);
    EXPECT_DOUBLE_EQ(msg.at("x").as_double(), position.x);
    EXPECT_DOUBLE_EQ(msg.at("y").as_double(), position.y);
    EXPECT_DOUBLE_EQ(msg.at("width").as_double(), dimensions.x);
    EXPECT_DOUBLE_EQ(msg.at("height").as_double(), dimensions.y);
    EXPECT_EQ(msg.at("filepath").as_string(), "sprites/tree.png");
}

TEST_F(GameplaySpecTest, DestroyObject2DRemovesObjectAndBroadcasts)
{
    setUpRoom();
    auto* room = server::RoomStorage::getRoomById(ROOM_ID);
    room->addObject(Object2D(OBJECT_ID, Vector2f(1.0f, 2.0f), Vector2f(3.0f, 4.0f)));

    auto targetMessages = receivedMessages(*m_target);
    ASSERT_EQ(read(destroyObject2DBytes(OBJECT_ID), *m_sender), server::CommandResult::success);

    EXPECT_TRUE(room->getObjects2D().empty());

    ASSERT_EQ(targetMessages.size(), 1);
    const auto& msg = targetMessages[0];
    EXPECT_EQ(msg.at("type").as_string(), "object_2D");
    EXPECT_EQ(msg.at("action").as_string(), "destroy");
    EXPECT_EQ(Util::toUint64(msg.at("id")), OBJECT_ID);
}

TEST_F(GameplaySpecTest, MoveObject2DOffsetsPositionAndBroadcasts)
{
    setUpRoom();
    auto* room = server::RoomStorage::getRoomById(ROOM_ID);
    room->addObject(Object2D(OBJECT_ID, Vector2f(1.0f, 2.0f), Vector2f(3.0f, 4.0f)));

    auto targetMessages = receivedMessages(*m_target);
    const Vector2f offset(5.0f, -2.0f);
    ASSERT_EQ(read(moveObject2DBytes(OBJECT_ID, offset), *m_sender), server::CommandResult::success);

    const auto& object = room->getObjects2D()[0];
    EXPECT_FLOAT_EQ(object.getPosition().x, 1.0f + offset.x);
    EXPECT_FLOAT_EQ(object.getPosition().y, 2.0f + offset.y);

    ASSERT_EQ(targetMessages.size(), 1);
    const auto& msg = targetMessages[0];
    EXPECT_EQ(msg.at("action").as_string(), "move");
    EXPECT_EQ(Util::toUint64(msg.at("id")), OBJECT_ID);
    EXPECT_DOUBLE_EQ(msg.at("x").as_double(), 1.0f + offset.x);
    EXPECT_DOUBLE_EQ(msg.at("y").as_double(), 2.0f + offset.y);
}

TEST_F(GameplaySpecTest, MoveObject2DUnknownObjectFails)
{
    setUpRoom();

    EXPECT_EQ(read(moveObject2DBytes(OBJECT_ID, Vector2f(1.0f, 1.0f)), *m_sender),
              server::CommandResult::failure);
}

TEST_F(GameplaySpecTest, CreateObject3DAddsStaticObjectAndBroadcasts)
{
    setUpRoom();
    auto targetMessages = receivedMessages(*m_target);

    const Vector3f position(1.0f, 2.0f, 3.0f);
    const Vector3f dimensions(4.0f, 5.0f, 6.0f);
    ASSERT_EQ(read(createObject3DBytes(position, dimensions, "models/house.obj"), *m_sender),
              server::CommandResult::success);

    auto* room = server::RoomStorage::getRoomById(ROOM_ID);
    ASSERT_EQ(room->getObjects3D().size(), 1);
    const auto& object = room->getObjects3D()[0];
    EXPECT_FLOAT_EQ(object.getPosition().x, position.x);
    EXPECT_FLOAT_EQ(object.getPosition().y, position.y);
    EXPECT_FLOAT_EQ(object.getPosition().z, position.z);
    EXPECT_FLOAT_EQ(object.getDimensions().x, dimensions.x);
    EXPECT_FLOAT_EQ(object.getDimensions().y, dimensions.y);
    EXPECT_FLOAT_EQ(object.getDimensions().z, dimensions.z);
    EXPECT_EQ(object.getFilepath(), "models/house.obj");

    ASSERT_EQ(targetMessages.size(), 1);
    const auto& msg = targetMessages[0];
    EXPECT_EQ(msg.at("type").as_string(), "object_3D");
    EXPECT_EQ(msg.at("action").as_string(), "create");
    EXPECT_EQ(Util::toUint64(msg.at("id")), object.getId());
    EXPECT_EQ(Util::toUint64(msg.at("callback")), MESSAGE_ID);
    EXPECT_DOUBLE_EQ(msg.at("x").as_double(), position.x);
    EXPECT_DOUBLE_EQ(msg.at("y").as_double(), position.y);
    EXPECT_DOUBLE_EQ(msg.at("z").as_double(), position.z);
    EXPECT_DOUBLE_EQ(msg.at("w").as_double(), dimensions.x);
    EXPECT_DOUBLE_EQ(msg.at("h").as_double(), dimensions.y);
    EXPECT_DOUBLE_EQ(msg.at("d").as_double(), dimensions.z);
    EXPECT_EQ(msg.at("filepath").as_string(), "models/house.obj");
}

TEST_F(GameplaySpecTest, DestroyObject3DRemovesObjectAndBroadcasts)
{
    setUpRoom();
    auto* room = server::RoomStorage::getRoomById(ROOM_ID);
    room->addObject(Object3D(OBJECT_ID, Vector3f(1.0f, 2.0f, 3.0f), Vector3f(4.0f, 5.0f, 6.0f)));

    auto targetMessages = receivedMessages(*m_target);
    ASSERT_EQ(read(destroyObject3DBytes(OBJECT_ID), *m_sender), server::CommandResult::success);

    EXPECT_TRUE(room->getObjects3D().empty());

    ASSERT_EQ(targetMessages.size(), 1);
    const auto& msg = targetMessages[0];
    EXPECT_EQ(msg.at("type").as_string(), "object_3D");
    EXPECT_EQ(msg.at("action").as_string(), "destroy");
    EXPECT_EQ(Util::toUint64(msg.at("id")), OBJECT_ID);
}

TEST_F(GameplaySpecTest, MoveObject3DOffsetsPositionAndBroadcasts)
{
    setUpRoom();
    auto* room = server::RoomStorage::getRoomById(ROOM_ID);
    room->addObject(Object3D(OBJECT_ID, Vector3f(1.0f, 2.0f, 3.0f), Vector3f(4.0f, 5.0f, 6.0f)));

    auto targetMessages = receivedMessages(*m_target);
    const Vector3f offset(5.0f, -2.0f, 1.0f);
    ASSERT_EQ(read(moveObject3DBytes(OBJECT_ID, offset), *m_sender), server::CommandResult::success);

    const auto& object = room->getObjects3D()[0];
    EXPECT_FLOAT_EQ(object.getPosition().x, 1.0f + offset.x);
    EXPECT_FLOAT_EQ(object.getPosition().y, 2.0f + offset.y);
    EXPECT_FLOAT_EQ(object.getPosition().z, 3.0f + offset.z);

    ASSERT_EQ(targetMessages.size(), 1);
    const auto& msg = targetMessages[0];
    EXPECT_EQ(msg.at("action").as_string(), "move");
    EXPECT_EQ(Util::toUint64(msg.at("id")), OBJECT_ID);
    EXPECT_DOUBLE_EQ(msg.at("x").as_double(), 1.0f + offset.x);
    EXPECT_DOUBLE_EQ(msg.at("y").as_double(), 2.0f + offset.y);
    EXPECT_DOUBLE_EQ(msg.at("z").as_double(), 3.0f + offset.z);
}

TEST_F(GameplaySpecTest, MoveObject3DUnknownObjectFails)
{
    setUpRoom();

    EXPECT_EQ(read(moveObject3DBytes(OBJECT_ID, Vector3f(1.0f, 1.0f, 1.0f)), *m_sender),
              server::CommandResult::failure);
}

// ---------------------------------------------------------------------------
// Movement function math
// ---------------------------------------------------------------------------

TEST(MovementFunctionTest, EasingIsQuadratic)
{
    EXPECT_DOUBLE_EQ(server::Movement::easing(0.0, 100.0), 0.0);
    EXPECT_DOUBLE_EQ(server::Movement::easing(0.5, 100.0), 25.0);
    EXPECT_DOUBLE_EQ(server::Movement::easing(1.0, 100.0), 100.0);
    EXPECT_DOUBLE_EQ(server::Movement::easing(0.5, 0.0), 0.0);
}

TEST(MovementFunctionTest, LinearScalesByProgress)
{
    EXPECT_DOUBLE_EQ(server::Movement::linear(0.0, 100.0), 0.0);
    EXPECT_DOUBLE_EQ(server::Movement::linear(0.5, 100.0), 50.0);
    EXPECT_DOUBLE_EQ(server::Movement::linear(1.0, 100.0), 100.0);
}

// ---------------------------------------------------------------------------
// Client-side parsing of shoot / respawn / leaderboard / moving objects
// ---------------------------------------------------------------------------

TEST(ClientGameplayParseTest, ParsesShootIntoDamageEvent)
{
    client::ClientConfig config;
    client::ClientAPI api(config);
    api.setClientId(SENDER_ID);

    boost::json::object json;
    json["command"] = "room";
    json["type"] = "player_3D";
    json["callback"] = 0;
    json["action"] = "shoot";
    json["room_id"] = ROOM_ID;
    json["client_id"] = SENDER_ID;
    json["target_id"] = TARGET_ID;
    json["damage"] = 25.5;
    json["new_health"] = 74.5;

    ASSERT_EQ(readClientMessage(api, json), client::ReadResult::success);

    auto events = api.consumeDamageEvents();
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].target_id, TARGET_ID);
    EXPECT_FLOAT_EQ(events[0].damage, 25.5f);
    EXPECT_FLOAT_EQ(events[0].new_health, 74.5f);
}

TEST(ClientGameplayParseTest, ParsesRespawnIntoRespawnEvent)
{
    client::ClientConfig config;
    client::ClientAPI api(config);
    api.setClientId(SENDER_ID);

    boost::json::object json;
    json["command"] = "room";
    json["type"] = "player_3D";
    json["callback"] = 0;
    json["action"] = "respawn";
    json["room_id"] = ROOM_ID;
    json["client_id"] = SENDER_ID;
    json["target_id"] = TARGET_ID;
    json["x"] = 10.0;
    json["y"] = 0.0;
    json["z"] = 0.0;

    ASSERT_EQ(readClientMessage(api, json), client::ReadResult::success);

    auto events = api.consumeRespawnEvents();
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events[0].target_id, TARGET_ID);
}

TEST(ClientGameplayParseTest, ParsesLeaderboardEntries)
{
    client::ClientConfig config;
    client::ClientAPI api(config);
    api.setClientId(SENDER_ID);

    boost::json::object json;
    json["command"] = "room";
    json["type"] = "player_3D";
    json["callback"] = 0;
    json["action"] = "leaderboard";
    json["room_id"] = ROOM_ID;
    json["client_id"] = SENDER_ID;

    boost::json::array entries;
    boost::json::object killer;
    killer["id"] = SENDER_ID;
    killer["username"] = "shooters";
    killer["team"] = "Terrorist";
    killer["kills"] = 3;
    killer["deaths"] = 1;
    entries.emplace_back(std::move(killer));
    boost::json::object victim;
    victim["id"] = TARGET_ID;
    victim["username"] = "victims";
    victim["team"] = "Counter Terrorist";
    victim["kills"] = 1;
    victim["deaths"] = 4;
    entries.emplace_back(std::move(victim));
    json["entries"] = std::move(entries);

    ASSERT_EQ(readClientMessage(api, json), client::ReadResult::success);

    auto events = api.consumeLeaderboardEvents();
    ASSERT_EQ(events.size(), 1);
    ASSERT_EQ(events[0].entries.size(), 2);

    const auto& first = events[0].entries[0];
    EXPECT_EQ(first.id, SENDER_ID);
    EXPECT_EQ(first.username, "shooters");
    EXPECT_EQ(first.team, "Terrorist");
    EXPECT_EQ(first.kills, 3);
    EXPECT_EQ(first.deaths, 1);

    const auto& second = events[0].entries[1];
    EXPECT_EQ(second.id, TARGET_ID);
    EXPECT_EQ(second.username, "victims");
    EXPECT_EQ(second.team, "Counter Terrorist");
    EXPECT_EQ(second.kills, 1);
    EXPECT_EQ(second.deaths, 4);
}

TEST(ClientGameplayParseTest, ParsesCreateMovingObject2D)
{
    client::ClientConfig config;
    client::ClientAPI api(config);
    api.setClientId(SENDER_ID);

    ASSERT_EQ(readClientMessage(api, infoRoomsMessage()), client::ReadResult::success);

    boost::json::object json;
    json["command"] = "room";
    json["type"] = "object_2D";
    json["callback"] = 0;
    json["action"] = "create_moving";
    json["room_id"] = ROOM_ID;
    json["id"] = OBJECT_ID;
    json["x"] = 10.0;
    json["y"] = 20.0;
    json["width"] = 2.0;
    json["height"] = 3.0;
    json["filepath"] = "sprites/ball.png";

    ASSERT_EQ(readClientMessage(api, json), client::ReadResult::success);

    auto* room = api.getRoom(ROOM_ID);
    ASSERT_NE(room, nullptr);
    ASSERT_EQ(room->getObjects2D().size(), 1);
    const auto& object = room->getObjects2D()[0];
    EXPECT_EQ(object.getId(), OBJECT_ID);
    EXPECT_FLOAT_EQ(object.getPosition().x, 10.0f);
    EXPECT_FLOAT_EQ(object.getPosition().y, 20.0f);
    EXPECT_FLOAT_EQ(object.getDimensions().x, 2.0f);
    EXPECT_FLOAT_EQ(object.getDimensions().y, 3.0f);
    EXPECT_EQ(object.getFilepath(), "sprites/ball.png");
}

TEST(ClientGameplayParseTest, ParsesCreateMovingObject3D)
{
    client::ClientConfig config;
    client::ClientAPI api(config);
    api.setClientId(SENDER_ID);

    ASSERT_EQ(readClientMessage(api, infoRoomsMessage()), client::ReadResult::success);

    boost::json::object json;
    json["command"] = "room";
    json["type"] = "object_3D";
    json["callback"] = 0;
    json["action"] = "create_moving";
    json["room_id"] = ROOM_ID;
    json["id"] = OBJECT_ID;
    json["x"] = 1.0;
    json["y"] = 2.0;
    json["z"] = 3.0;
    json["w"] = 4.0;
    json["h"] = 5.0;
    json["d"] = 6.0;
    json["filepath"] = "models/crate.obj";

    ASSERT_EQ(readClientMessage(api, json), client::ReadResult::success);

    auto snapshots = api.getRemoteObjects3DSnapshot(ROOM_ID);
    ASSERT_EQ(snapshots.size(), 1);
    EXPECT_EQ(snapshots[0].id, OBJECT_ID);
    EXPECT_FLOAT_EQ(snapshots[0].x, 1.0f);
    EXPECT_FLOAT_EQ(snapshots[0].y, 2.0f);
    EXPECT_FLOAT_EQ(snapshots[0].z, 3.0f);
    EXPECT_FLOAT_EQ(snapshots[0].w, 4.0f);
    EXPECT_FLOAT_EQ(snapshots[0].h, 5.0f);
    EXPECT_FLOAT_EQ(snapshots[0].d, 6.0f);
}

TEST(ClientGameplayParseTest, ParsesCreateMovingObject2DUpdate)
{
    client::ClientConfig config;
    client::ClientAPI api(config);
    api.setClientId(SENDER_ID);

    ASSERT_EQ(readClientMessage(api, infoRoomsMessage()), client::ReadResult::success);

    // Seed a moving object with the server's create broadcast.
    boost::json::object create;
    create["command"] = "room";
    create["type"] = "object_2D";
    create["callback"] = 0;
    create["action"] = "create_moving";
    create["room_id"] = ROOM_ID;
    create["createMovingType"] = "create";
    create["id"] = OBJECT_ID;
    create["x"] = 0.0;
    create["y"] = 0.0;
    create["width"] = 2.0;
    create["height"] = 3.0;
    create["filepath"] = "sprites/ball.png";
    ASSERT_EQ(readClientMessage(api, create), client::ReadResult::success);

    auto* room = api.getRoom(ROOM_ID);
    ASSERT_NE(room, nullptr);
    ASSERT_EQ(room->getObjects2D().size(), 1);

    // A movement update reuses the same id/x/y keys; it must move the existing
    // object instead of creating a new one.
    boost::json::object update;
    update["command"] = "room";
    update["type"] = "object_2D";
    update["callback"] = 0;
    update["action"] = "create_moving";
    update["room_id"] = ROOM_ID;
    update["createMovingType"] = "update";
    update["id"] = OBJECT_ID;
    update["x"] = 7.0;
    update["y"] = 8.0;
    ASSERT_EQ(readClientMessage(api, update), client::ReadResult::success);

    ASSERT_EQ(room->getObjects2D().size(), 1);
    const auto& object = room->getObjects2D()[0];
    EXPECT_EQ(object.getId(), OBJECT_ID);
    EXPECT_FLOAT_EQ(object.getPosition().x, 7.0f);
    EXPECT_FLOAT_EQ(object.getPosition().y, 8.0f);
    // An update must not clobber the object's dimensions or filepath.
    EXPECT_FLOAT_EQ(object.getDimensions().x, 2.0f);
    EXPECT_FLOAT_EQ(object.getDimensions().y, 3.0f);
    EXPECT_EQ(object.getFilepath(), "sprites/ball.png");
}

TEST(ClientGameplayParseTest, ParsesCreateMovingObject3DUpdate)
{
    client::ClientConfig config;
    client::ClientAPI api(config);
    api.setClientId(SENDER_ID);

    ASSERT_EQ(readClientMessage(api, infoRoomsMessage()), client::ReadResult::success);

    boost::json::object create;
    create["command"] = "room";
    create["type"] = "object_3D";
    create["callback"] = 0;
    create["action"] = "create_moving";
    create["room_id"] = ROOM_ID;
    create["createMovingType"] = "create";
    create["id"] = OBJECT_ID;
    create["x"] = 0.0;
    create["y"] = 0.0;
    create["z"] = 0.0;
    create["w"] = 4.0;
    create["h"] = 5.0;
    create["d"] = 6.0;
    create["filepath"] = "models/crate.obj";
    ASSERT_EQ(readClientMessage(api, create), client::ReadResult::success);

    boost::json::object update;
    update["command"] = "room";
    update["type"] = "object_3D";
    update["callback"] = 0;
    update["action"] = "create_moving";
    update["room_id"] = ROOM_ID;
    update["createMovingType"] = "update";
    update["id"] = OBJECT_ID;
    update["x"] = 9.0;
    update["y"] = 10.0;
    update["z"] = 11.0;
    ASSERT_EQ(readClientMessage(api, update), client::ReadResult::success);

    auto snapshots = api.getRemoteObjects3DSnapshot(ROOM_ID);
    ASSERT_EQ(snapshots.size(), 1);
    EXPECT_EQ(snapshots[0].id, OBJECT_ID);
    EXPECT_FLOAT_EQ(snapshots[0].x, 9.0f);
    EXPECT_FLOAT_EQ(snapshots[0].y, 10.0f);
    EXPECT_FLOAT_EQ(snapshots[0].z, 11.0f);
    // An update must not clobber the object's dimensions.
    EXPECT_FLOAT_EQ(snapshots[0].w, 4.0f);
    EXPECT_FLOAT_EQ(snapshots[0].h, 5.0f);
    EXPECT_FLOAT_EQ(snapshots[0].d, 6.0f);
}

TEST(ClientGameplayParseTest, ParsesObject2DCreateDestroyMove)
{
    client::ClientConfig config;
    client::ClientAPI api(config);
    api.setClientId(SENDER_ID);

    ASSERT_EQ(readClientMessage(api, infoRoomsMessage()), client::ReadResult::success);

    // Creating a static object adds it to the local room.
    boost::json::object create;
    create["command"] = "room";
    create["type"] = "object_2D";
    create["callback"] = 0;
    create["action"] = "create";
    create["room_id"] = ROOM_ID;
    create["id"] = OBJECT_ID;
    create["x"] = 1.0;
    create["y"] = 2.0;
    create["width"] = 3.0;
    create["height"] = 4.0;
    create["filepath"] = "sprites/tree.png";

    ASSERT_EQ(readClientMessage(api, create), client::ReadResult::success);

    auto* room = api.getRoom(ROOM_ID);
    ASSERT_NE(room, nullptr);
    ASSERT_EQ(room->getObjects2D().size(), 1);
    EXPECT_EQ(room->getObjects2D()[0].getId(), OBJECT_ID);
    EXPECT_EQ(room->getObjects2D()[0].getFilepath(), "sprites/tree.png");

    // A move action updates the object position.
    boost::json::object move;
    move["command"] = "room";
    move["type"] = "object_2D";
    move["callback"] = 0;
    move["action"] = "move";
    move["room_id"] = ROOM_ID;
    move["id"] = OBJECT_ID;
    move["x"] = 10.0;
    move["y"] = 20.0;

    ASSERT_EQ(readClientMessage(api, move), client::ReadResult::success);
    EXPECT_FLOAT_EQ(room->getObjects2D()[0].getPosition().x, 10.0f);
    EXPECT_FLOAT_EQ(room->getObjects2D()[0].getPosition().y, 20.0f);

    // A destroy action removes the object again.
    boost::json::object destroy;
    destroy["command"] = "room";
    destroy["type"] = "object_2D";
    destroy["callback"] = 0;
    destroy["action"] = "destroy";
    destroy["room_id"] = ROOM_ID;
    destroy["id"] = OBJECT_ID;

    ASSERT_EQ(readClientMessage(api, destroy), client::ReadResult::success);
    EXPECT_TRUE(room->getObjects2D().empty());
}

TEST(ClientGameplayParseTest, ParsesObject3DCreateDestroyMove)
{
    client::ClientConfig config;
    client::ClientAPI api(config);
    api.setClientId(SENDER_ID);

    ASSERT_EQ(readClientMessage(api, infoRoomsMessage()), client::ReadResult::success);

    boost::json::object create;
    create["command"] = "room";
    create["type"] = "object_3D";
    create["callback"] = 0;
    create["action"] = "create";
    create["room_id"] = ROOM_ID;
    create["id"] = OBJECT_ID;
    create["x"] = 1.0;
    create["y"] = 2.0;
    create["z"] = 3.0;
    create["w"] = 4.0;
    create["h"] = 5.0;
    create["d"] = 6.0;
    create["filepath"] = "models/house.obj";

    ASSERT_EQ(readClientMessage(api, create), client::ReadResult::success);

    auto snapshots = api.getRemoteObjects3DSnapshot(ROOM_ID);
    ASSERT_EQ(snapshots.size(), 1);
    EXPECT_EQ(snapshots[0].id, OBJECT_ID);

    boost::json::object move;
    move["command"] = "room";
    move["type"] = "object_3D";
    move["callback"] = 0;
    move["action"] = "move";
    move["room_id"] = ROOM_ID;
    move["id"] = OBJECT_ID;
    move["x"] = 10.0;
    move["y"] = 20.0;
    move["z"] = 30.0;

    ASSERT_EQ(readClientMessage(api, move), client::ReadResult::success);
    auto moved = api.getRemoteObjects3DSnapshot(ROOM_ID);
    ASSERT_EQ(moved.size(), 1);
    EXPECT_FLOAT_EQ(moved[0].x, 10.0f);
    EXPECT_FLOAT_EQ(moved[0].y, 20.0f);
    EXPECT_FLOAT_EQ(moved[0].z, 30.0f);

    boost::json::object destroy;
    destroy["command"] = "room";
    destroy["type"] = "object_3D";
    destroy["callback"] = 0;
    destroy["action"] = "destroy";
    destroy["room_id"] = ROOM_ID;
    destroy["id"] = OBJECT_ID;

    ASSERT_EQ(readClientMessage(api, destroy), client::ReadResult::success);
    EXPECT_TRUE(api.getRemoteObjects3DSnapshot(ROOM_ID).empty());
}

TEST(ClientGameplayParseTest, ParsesPlayer3DDimensions)
{
    client::ClientConfig config;
    client::ClientAPI api(config);
    api.setClientId(SENDER_ID);

    // The room already reports the remote player as a member.
    ASSERT_EQ(readClientMessage(api, infoRoomsWithClientsMessage({TARGET_ID})), client::ReadResult::success);

    boost::json::object json;
    json["command"] = "room";
    json["type"] = "player_3D";
    json["callback"] = 0;
    json["action"] = "dimensions";
    json["room_id"] = ROOM_ID;
    json["client_id"] = TARGET_ID;
    json["x"] = 2.0;
    json["y"] = 4.0;
    json["z"] = 6.0;

    ASSERT_EQ(readClientMessage(api, json), client::ReadResult::success);

    auto players = api.getRemotePlayersSnapshot(ROOM_ID, SENDER_ID);
    ASSERT_EQ(players.size(), 1);
    EXPECT_EQ(players[0].id, TARGET_ID);
    EXPECT_FLOAT_EQ(players[0].w, 2.0f);
    EXPECT_FLOAT_EQ(players[0].h, 4.0f);
    EXPECT_FLOAT_EQ(players[0].d, 6.0f);
}

TEST(ClientGameplayParseTest, ParsesPlayer2DPositionAndDimension)
{
    client::ClientConfig config;
    client::ClientAPI api(config);
    api.setClientId(SENDER_ID);

    ASSERT_EQ(readClientMessage(api, infoRoomsWithClientsMessage({TARGET_ID})), client::ReadResult::success);

    boost::json::object position;
    position["command"] = "room";
    position["type"] = "player_2D";
    position["callback"] = 0;
    position["action"] = "position";
    position["room_id"] = ROOM_ID;
    position["client_id"] = TARGET_ID;
    position["x"] = 15.0;
    position["y"] = 25.0;

    ASSERT_EQ(readClientMessage(api, position), client::ReadResult::success);

    auto* room = api.getRoom(ROOM_ID);
    ASSERT_NE(room, nullptr);
    auto& clients = room->getClients();
    ASSERT_EQ(clients.size(), 1);
    EXPECT_EQ(clients[0].getId(), TARGET_ID);
    EXPECT_FLOAT_EQ(clients[0].getObject2D().getPosition().x, 15.0f);
    EXPECT_FLOAT_EQ(clients[0].getObject2D().getPosition().y, 25.0f);

    boost::json::object dimension;
    dimension["command"] = "room";
    dimension["type"] = "player_2D";
    dimension["callback"] = 0;
    dimension["action"] = "dimension";
    dimension["room_id"] = ROOM_ID;
    dimension["client_id"] = TARGET_ID;
    dimension["x"] = 3.0;
    dimension["y"] = 5.0;

    ASSERT_EQ(readClientMessage(api, dimension), client::ReadResult::success);
    EXPECT_FLOAT_EQ(clients[0].getObject2D().getDimensions().x, 3.0f);
    EXPECT_FLOAT_EQ(clients[0].getObject2D().getDimensions().y, 5.0f);
}

// ---------------------------------------------------------------------------
// Packet wire formats
// ---------------------------------------------------------------------------

TEST(GameplayPacketWireFormatTest, Shoot)
{
    client::ClientConfig config;
    client::ClientAPI api(config);
    api.setClientId(1);

    auto bytes = client::Packet::Room::Player3D::shoot(api, TARGET_ID, 25.0f);
    ASSERT_EQ(bytes.size(), 16 + 3 + 8 + 4);
    EXPECT_EQ(bytes[16], static_cast<uint8_t>(CommandType::room));
    EXPECT_EQ(bytes[17], static_cast<uint8_t>(RoomCommandType::Root::player_3D));
    EXPECT_EQ(bytes[18], static_cast<uint8_t>(RoomCommandType::PlayerType::shoot));

    nx_data targetBytes(bytes.begin() + 19, bytes.begin() + 27);
    EXPECT_EQ(Util::uint64FromFront(targetBytes), TARGET_ID);
    nx_data damageBytes(bytes.begin() + 27, bytes.begin() + 31);
    EXPECT_FLOAT_EQ(Util::floatFromFront(damageBytes), 25.0f);
}

TEST(GameplayPacketWireFormatTest, SetTeam)
{
    client::ClientConfig config;
    client::ClientAPI api(config);
    api.setClientId(1);

    auto bytes = client::Packet::Room::Player3D::setTeam(api, "Terrorist");
    ASSERT_EQ(bytes.size(), 16 + 3 + 9);
    EXPECT_EQ(bytes[16], static_cast<uint8_t>(CommandType::room));
    EXPECT_EQ(bytes[17], static_cast<uint8_t>(RoomCommandType::Root::player_3D));
    EXPECT_EQ(bytes[18], static_cast<uint8_t>(RoomCommandType::PlayerType::set_team));
    ASSERT_GE(bytes.size(), 16 + 3 + 9);
    EXPECT_EQ(bytes[19], 'T');
    EXPECT_EQ(bytes[27], 't');
}

TEST(GameplayPacketWireFormatTest, CreateMoving2D)
{
    client::ClientConfig config;
    client::ClientAPI api(config);
    api.setClientId(1);

    const Vector2f start(1.0f, 2.0f);
    const Vector2f dims(3.0f, 4.0f);
    const Vector2f movement(5.0f, 6.0f);
    auto bytes = client::Packet::Room::Object2D::createMoving(api, start, dims, movement, 0.016f,
                                                              MovementType::eased, "a.png");
    ASSERT_EQ(bytes.size(), 16 + 3 + 8 + 8 + 8 + 4 + 1 + 5);
    EXPECT_EQ(bytes[16], static_cast<uint8_t>(CommandType::room));
    EXPECT_EQ(bytes[17], static_cast<uint8_t>(RoomCommandType::Root::object_2D));
    EXPECT_EQ(bytes[18], static_cast<uint8_t>(RoomCommandType::ObjectType::create_moving));

    // Decode the payload the same way the server command does.
    nx_data commandBytes(bytes.begin() + 16, bytes.end());
    auto payload = Util::removeAmountOfBytesFromVector(commandBytes, 3);
    auto startPos = Util::vector2fFromFront(payload);
    payload = Util::removeAmountOfBytesFromVector(payload, 8);
    auto dimsPos = Util::vector2fFromFront(payload);
    payload = Util::removeAmountOfBytesFromVector(payload, 8);
    auto movePos = Util::vector2fFromFront(payload);
    payload = Util::removeAmountOfBytesFromVector(payload, 8);
    auto delta = Util::floatFromFront(payload);
    EXPECT_FLOAT_EQ(startPos.x, start.x);
    EXPECT_FLOAT_EQ(startPos.y, start.y);
    EXPECT_FLOAT_EQ(dimsPos.x, dims.x);
    EXPECT_FLOAT_EQ(dimsPos.y, dims.y);
    EXPECT_FLOAT_EQ(movePos.x, movement.x);
    EXPECT_FLOAT_EQ(movePos.y, movement.y);
    EXPECT_FLOAT_EQ(delta, 0.016f);
    EXPECT_EQ(payload[4], static_cast<uint8_t>(MovementType::eased));
}

TEST(GameplayPacketWireFormatTest, CreateMoving3D)
{
    client::ClientConfig config;
    client::ClientAPI api(config);
    api.setClientId(1);

    const Vector3f start(1.0f, 2.0f, 3.0f);
    const Vector3f dims(4.0f, 5.0f, 6.0f);
    const Vector3f movement(7.0f, 8.0f, 9.0f);
    auto bytes = client::Packet::Room::Object3D::createMoving(api, start, dims, movement, 0.033f,
                                                              MovementType::linear, "b.obj");
    ASSERT_EQ(bytes.size(), 16 + 3 + 12 + 12 + 12 + 4 + 1 + 5);
    EXPECT_EQ(bytes[16], static_cast<uint8_t>(CommandType::room));
    EXPECT_EQ(bytes[17], static_cast<uint8_t>(RoomCommandType::Root::object_3D));
    EXPECT_EQ(bytes[18], static_cast<uint8_t>(RoomCommandType::ObjectType::create_moving));

    nx_data commandBytes(bytes.begin() + 16, bytes.end());
    auto payload = Util::removeAmountOfBytesFromVector(commandBytes, 3);
    auto startPos = Util::vector3fFromFront(payload);
    payload = Util::removeAmountOfBytesFromVector(payload, 12);
    auto dimsPos = Util::vector3fFromFront(payload);
    payload = Util::removeAmountOfBytesFromVector(payload, 12);
    auto movePos = Util::vector3fFromFront(payload);
    payload = Util::removeAmountOfBytesFromVector(payload, 12);
    auto delta = Util::floatFromFront(payload);
    EXPECT_FLOAT_EQ(startPos.x, start.x);
    EXPECT_FLOAT_EQ(startPos.y, start.y);
    EXPECT_FLOAT_EQ(startPos.z, start.z);
    EXPECT_FLOAT_EQ(dimsPos.x, dims.x);
    EXPECT_FLOAT_EQ(dimsPos.y, dims.y);
    EXPECT_FLOAT_EQ(dimsPos.z, dims.z);
    EXPECT_FLOAT_EQ(movePos.x, movement.x);
    EXPECT_FLOAT_EQ(movePos.y, movement.y);
    EXPECT_FLOAT_EQ(movePos.z, movement.z);
    EXPECT_FLOAT_EQ(delta, 0.033f);
    EXPECT_EQ(payload[4], static_cast<uint8_t>(MovementType::linear));
}

TEST(GameplayPacketWireFormatTest, Player3DPositionAndDimension)
{
    client::ClientConfig config;
    client::ClientAPI api(config);
    api.setClientId(1);

    const Vector3f vector(1.0f, 2.0f, 3.0f);

    auto position = client::Packet::Room::Player3D::position(api, vector);
    ASSERT_EQ(position.size(), 16 + 3 + 12);
    EXPECT_EQ(position[16], static_cast<uint8_t>(CommandType::room));
    EXPECT_EQ(position[17], static_cast<uint8_t>(RoomCommandType::Root::player_3D));
    EXPECT_EQ(position[18], static_cast<uint8_t>(RoomCommandType::PlayerType::position));
    nx_data cmdBytes(position.begin() + 16, position.end());
    auto payload = Util::removeAmountOfBytesFromVector(cmdBytes, 3);
    auto parsed = Util::vector3fFromFront(payload);
    EXPECT_FLOAT_EQ(parsed.x, vector.x);
    EXPECT_FLOAT_EQ(parsed.y, vector.y);
    EXPECT_FLOAT_EQ(parsed.z, vector.z);

    auto positionWithYaw = client::Packet::Room::Player3D::position(api, vector, 1.25f);
    ASSERT_EQ(positionWithYaw.size(), 16 + 3 + 16);
    nx_data yawBytes(positionWithYaw.begin() + 16 + 3 + 12, positionWithYaw.end());
    EXPECT_FLOAT_EQ(Util::floatFromFront(yawBytes), 1.25f);

    auto dimension = client::Packet::Room::Player3D::dimension(api, vector);
    ASSERT_EQ(dimension.size(), 16 + 3 + 12);
    EXPECT_EQ(dimension[16], static_cast<uint8_t>(CommandType::room));
    EXPECT_EQ(dimension[17], static_cast<uint8_t>(RoomCommandType::Root::player_3D));
    EXPECT_EQ(dimension[18], static_cast<uint8_t>(RoomCommandType::PlayerType::dimension));

    auto movementWithYaw = client::Packet::Room::Player3D::movement(api, vector, 0.02f, -0.75f);
    ASSERT_EQ(movementWithYaw.size(), 16 + 3 + 20);
    nx_data movementYawBytes(movementWithYaw.begin() + 16 + 3 + 16,
                             movementWithYaw.end());
    EXPECT_FLOAT_EQ(Util::floatFromFront(movementYawBytes), -0.75f);
}

TEST(GameplayPacketWireFormatTest, Player2DPositionAndDimension)
{
    client::ClientConfig config;
    client::ClientAPI api(config);
    api.setClientId(1);

    const Vector2f vector(1.0f, 2.0f);

    auto position = client::Packet::Room::Player2D::position(api, vector);
    ASSERT_EQ(position.size(), 16 + 3 + 8);
    EXPECT_EQ(position[16], static_cast<uint8_t>(CommandType::room));
    EXPECT_EQ(position[17], static_cast<uint8_t>(RoomCommandType::Root::player_2D));
    EXPECT_EQ(position[18], static_cast<uint8_t>(RoomCommandType::PlayerType::position));
    nx_data cmdBytes(position.begin() + 16, position.end());
    auto payload = Util::removeAmountOfBytesFromVector(cmdBytes, 3);
    auto parsed = Util::vector2fFromFront(payload);
    EXPECT_FLOAT_EQ(parsed.x, vector.x);
    EXPECT_FLOAT_EQ(parsed.y, vector.y);

    auto dimension = client::Packet::Room::Player2D::dimension(api, vector);
    ASSERT_EQ(dimension.size(), 16 + 3 + 8);
    EXPECT_EQ(dimension[16], static_cast<uint8_t>(CommandType::room));
    EXPECT_EQ(dimension[17], static_cast<uint8_t>(RoomCommandType::Root::player_2D));
    EXPECT_EQ(dimension[18], static_cast<uint8_t>(RoomCommandType::PlayerType::dimension));
}

TEST(GameplayPacketWireFormatTest, Object2DCreateDestroyMove)
{
    client::ClientConfig config;
    client::ClientAPI api(config);
    api.setClientId(1);

    const Vector2f position(1.0f, 2.0f);
    const Vector2f dimensions(3.0f, 4.0f);

    auto create = client::Packet::Room::Object2D::create(api, position, dimensions, "a.png");
    ASSERT_EQ(create.size(), 16 + 3 + 8 + 8 + 5);
    EXPECT_EQ(create[16], static_cast<uint8_t>(CommandType::room));
    EXPECT_EQ(create[17], static_cast<uint8_t>(RoomCommandType::Root::object_2D));
    EXPECT_EQ(create[18], static_cast<uint8_t>(RoomCommandType::ObjectType::create));
    nx_data cmdBytes(create.begin() + 16, create.end());
    auto payload = Util::removeAmountOfBytesFromVector(cmdBytes, 3);
    auto parsedPos = Util::vector2fFromFront(payload);
    payload = Util::removeAmountOfBytesFromVector(payload, 8);
    auto parsedDims = Util::vector2fFromFront(payload);
    EXPECT_FLOAT_EQ(parsedPos.x, position.x);
    EXPECT_FLOAT_EQ(parsedPos.y, position.y);
    EXPECT_FLOAT_EQ(parsedDims.x, dimensions.x);
    EXPECT_FLOAT_EQ(parsedDims.y, dimensions.y);

    auto destroy = client::Packet::Room::Object2D::destroy(api, OBJECT_ID);
    ASSERT_EQ(destroy.size(), 16 + 3 + 8);
    EXPECT_EQ(destroy[18], static_cast<uint8_t>(RoomCommandType::ObjectType::destroy));
    nx_data destroyCmd(destroy.begin() + 16, destroy.end());
    auto destroyPayload = Util::removeAmountOfBytesFromVector(destroyCmd, 3);
    EXPECT_EQ(Util::uint64FromFront(destroyPayload), OBJECT_ID);

    auto move = client::Packet::Room::Object2D::move(api, OBJECT_ID, Vector2f(5.0f, 6.0f));
    ASSERT_EQ(move.size(), 16 + 3 + 8 + 8);
    EXPECT_EQ(move[18], static_cast<uint8_t>(RoomCommandType::ObjectType::move));
    nx_data moveCmd(move.begin() + 16, move.end());
    auto movePayload = Util::removeAmountOfBytesFromVector(moveCmd, 3);
    EXPECT_EQ(Util::uint64FromFront(movePayload), OBJECT_ID);
    auto parsedMove = Util::vector2fFromFront(Util::removeAmountOfBytesFromVector(movePayload, 8));
    EXPECT_FLOAT_EQ(parsedMove.x, 5.0f);
    EXPECT_FLOAT_EQ(parsedMove.y, 6.0f);
}

TEST(GameplayPacketWireFormatTest, Object3DCreateDestroyMove)
{
    client::ClientConfig config;
    client::ClientAPI api(config);
    api.setClientId(1);

    const Vector3f position(1.0f, 2.0f, 3.0f);
    const Vector3f dimensions(4.0f, 5.0f, 6.0f);

    auto create = client::Packet::Room::Object3D::create(api, position, dimensions, "c.obj");
    ASSERT_EQ(create.size(), 16 + 3 + 12 + 12 + 5);
    EXPECT_EQ(create[16], static_cast<uint8_t>(CommandType::room));
    EXPECT_EQ(create[17], static_cast<uint8_t>(RoomCommandType::Root::object_3D));
    EXPECT_EQ(create[18], static_cast<uint8_t>(RoomCommandType::ObjectType::create));
    nx_data cmdBytes(create.begin() + 16, create.end());
    auto payload = Util::removeAmountOfBytesFromVector(cmdBytes, 3);
    auto parsedPos = Util::vector3fFromFront(payload);
    payload = Util::removeAmountOfBytesFromVector(payload, 12);
    auto parsedDims = Util::vector3fFromFront(payload);
    EXPECT_FLOAT_EQ(parsedPos.x, position.x);
    EXPECT_FLOAT_EQ(parsedPos.y, position.y);
    EXPECT_FLOAT_EQ(parsedPos.z, position.z);
    EXPECT_FLOAT_EQ(parsedDims.x, dimensions.x);
    EXPECT_FLOAT_EQ(parsedDims.y, dimensions.y);
    EXPECT_FLOAT_EQ(parsedDims.z, dimensions.z);

    auto destroy = client::Packet::Room::Object3D::destroy(api, OBJECT_ID);
    ASSERT_EQ(destroy.size(), 16 + 3 + 8);
    EXPECT_EQ(destroy[18], static_cast<uint8_t>(RoomCommandType::ObjectType::destroy));
    nx_data destroyCmd(destroy.begin() + 16, destroy.end());
    auto destroyPayload = Util::removeAmountOfBytesFromVector(destroyCmd, 3);
    EXPECT_EQ(Util::uint64FromFront(destroyPayload), OBJECT_ID);

    auto move = client::Packet::Room::Object3D::move(api, OBJECT_ID, Vector3f(5.0f, 6.0f, 7.0f));
    ASSERT_EQ(move.size(), 16 + 3 + 8 + 12);
    EXPECT_EQ(move[18], static_cast<uint8_t>(RoomCommandType::ObjectType::move));
    nx_data moveCmd(move.begin() + 16, move.end());
    auto movePayload = Util::removeAmountOfBytesFromVector(moveCmd, 3);
    EXPECT_EQ(Util::uint64FromFront(movePayload), OBJECT_ID);
    auto parsedMove = Util::vector3fFromFront(Util::removeAmountOfBytesFromVector(movePayload, 8));
    EXPECT_FLOAT_EQ(parsedMove.x, 5.0f);
    EXPECT_FLOAT_EQ(parsedMove.y, 6.0f);
    EXPECT_FLOAT_EQ(parsedMove.z, 7.0f);
}

} // namespace
