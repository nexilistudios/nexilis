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

#include <nexilis/json.hh>
#include <nexilis/logger/log.hh>
#include <nexilis/movement_type.hh>
#include <nexilis/nx_protocol.hh>
#include <nexilis/room_command_type.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/command/commands.hh>
#include <nexilis/server/movement.hh>
#include <nexilis/server/room_storage.hh>
#include <nexilis/server/server_json.hh>

#include <algorithm>
#include <array>
#include <chrono>
#include <mutex>
#include <unordered_map>

namespace nexilis::server
{

namespace
{
constexpr unsigned kMaxRootAuthFailures = 5;
constexpr auto kRootAuthWindow = std::chrono::minutes(10);
constexpr size_t kMaxTrackedRootAuthAddresses = 4096;
} // namespace

struct Command::RootAuthState
{
    struct Attempt
    {
        std::chrono::steady_clock::time_point firstFailure;
        unsigned failures = 0;
    };

    std::mutex mutex;
    std::unordered_map<std::string, Attempt> byAddress;
};

Command::Command(const ServerConfig& settings)
    : NxClass("server::Command"),
      m_settings(settings),
      m_rootAuthState(std::make_shared<RootAuthState>())
{
}

Command::Command(Command&& other)
    : NxClass(std::move(other)),
      m_settings(std::move(other.m_settings)),
      m_rootAuthState(std::move(other.m_rootAuthState))
{
}

Command& Command::operator=(Command&& other)
{
    if (this != &other)
    {
        m_settings = std::move(other.m_settings);
        m_rootAuthState = std::move(other.m_rootAuthState);
        NxClass::operator=(std::move(other));
    }
    return *this;
}

CommandResult Command::read(const char* command_data, size_t length, User& client, NXProtocol& protocol, uint64_t messageId)
{
    return Command::read(Util::convertToByteVector(command_data, length), client, protocol, messageId);
}

CommandResult Command::read(const nx_data& command, User& user, NXProtocol& protocol, uint64_t messageId)
{
    assert(user.getId() != 0);

    if (command.size() < 2)
    {
        return CommandResult::invalid_input;
    }

    // Missing optional command levels cannot match a valid action.
    std::array<uint8_t, 4> commandBytes;
    commandBytes.fill(255);
    std::copy_n(command.begin(), std::min(command.size(), commandBytes.size()), commandBytes.begin());

    auto arg = static_cast<CommandType>(commandBytes[0]);
    auto arg2 = commandBytes[1];
    auto arg3 = commandBytes[2];

    // Authentication payloads contain credentials, so do not dump their bytes.
    if (arg == CommandType::authentication)
    {
        Log::debug("Command: authentication payload redacted");
    }
    else
    {
        Log::debug("Command: Nexilis command sequence");
        Util::debugUint8Vector(command);
    }

    // DefaultArgs from Commands.
    DefaultArgs args(user, protocol, command, messageId);

    // Keep the movement system and the gameplay limits bound to the
    // live server settings: the static pointers must be refreshed on
    // every dispatch so a fresh Command (e.g. a recreated server or a
    // test fixture) does not leave detached movement threads reading a
    // destroyed configuration's tickrate or command handlers reading
    // destroyed limits.
    Movement::_initialize(m_settings);
    GameplayLimits::_initialize(m_settings.getGameplayLimits());

    Log::debug(header(), commandTypeAsString(arg));
    switch (arg)
    {
        case CommandType::setting:
        {
            switch (arg2)
            {
                // General settings.
                case 0:
                {
                    switch (arg3)
                    {
                        // Reset your client id.
                        // requires privileges.
                        case 0:
                        {
                            return ServerImpl::set_general_clientId(args);
                        }

                        // Set username to the client.
                        case 1:
                        {
                            return ServerImpl::set_general_username(args);
                        }
                    }
                    return CommandResult::not_found;
                }

                // Protocol specific setting
                case 1:
                {
                    switch (arg3)
                    {
                        // Boost TCP
                        case 0:
                        {
                            switch (commandBytes[3])
                            {
                                // Server address
                                case 0:
                                {
                                    Log::debug(header(), "setting::protocol::boostTCP::server_address");
                                    return CommandResult::unimplemented;
                                }

                                // Server port number
                                case 1:
                                {
                                    return ServerImpl::set_protocol_boosttcp_port(args);
                                }
                            }
                        }
                    }
                    return CommandResult::not_found;
                }
            }
            return CommandResult::not_found;
        }

        case CommandType::getting:
        {
            switch (arg2)
            {
                // General getting
                case 0:
                {
                    switch (arg3)
                    {
                        // Get client id.
                        case 0:
                        {
                            return ServerImpl::get_general_clientId(args);
                        }
                        case 1:
                        {
                            return ServerImpl::get_general_roomId(args);
                        }
                    }
                    return CommandResult::not_found;
                }
                // Server info type
                case 1:
                {
                    switch (arg3)
                    {
                        case 0:
                        {
                            return ServerImpl::get_info_general(args);
                        }
                        case 1:
                        {
                            return ServerImpl::get_info_clients(args);
                        }
                        case 2:
                        {
                            return ServerImpl::get_info_rooms(args);
                        }
                    }
                    return CommandResult::not_found;
                }
            }
            return CommandResult::not_found;
        }

        case CommandType::room:
        {
            switch (arg2)
            {
                // Management
                case 0:
                {
                    switch (arg3)
                    {
                        // Join room.
                        case 0:
                        {
                            return ServerImpl::room_management_join(args);
                        }

                        // Leave current room.
                        case 1:
                        {
                            return ServerImpl::room_management_leave(args);
                        }

                        /// Create room.
                        case 2:
                        {
                            return ServerImpl::room_management_create(args);
                        }

                        /// Remove room.
                        case 3:
                        {
                            return ServerImpl::room_management_remove(args);
                        }

                        /// Set overlapping (whether clients may share positions).
                        case 4:
                        {
                            return ServerImpl::room_management_setOverlap(args);
                        }
                    }
                    return CommandResult::not_found;
                }
                // Communicate
                case 1:
                {
                    switch (arg3)
                    {
                        // broadcast
                        case 0:
                        {
                            return ServerImpl::room_communicate_broadcast(args);
                        }

                        // broadcast with an end-to-end encrypted payload.
                        // The ciphertext is relayed verbatim; the server can
                        // not read it, only forward it to the room.
                        case 3:
                        {
                            return ServerImpl::room_communicate_broadcast(args);
                        }

                        // othercast
                        case 1:
                        {
                            return CommandResult::unimplemented;
                            // return ServerImpl::room_communicate_othercast(args);
                        }

                        // unicast
                        case 2:
                        {
                            return CommandResult::unimplemented;
                            // return ServerImpl::room_communicate_unicast(args);
                        }
                    }
                    return CommandResult::not_found;
                }
                // Player 2D
                case 2:
                {
                    switch (arg3)
                    {
                        // Position 2D
                        case 0:
                        {
                            return ServerImpl::room_player2d_position(args);
                        }

                        // Dimension 2D
                        case 1:
                        {
                            return ServerImpl::room_player2d_dimension(args);
                        }

                        // Movement 2D
                        case 2:
                        {
                            return ServerImpl::room_player2d_movement(args);
                        }

                        // Shoot 2D
                        case 3:
                        {
                            return ServerImpl::room_player2d_shoot(args);
                        }
                    }
                    return CommandResult::not_found;
                }

                // Object2D
                case 3:
                {
                    switch (arg3)
                    {
                        // Create object
                        case 0:
                        {
                            return ServerImpl::room_object2d_create(args);
                        }

                        // Delete object
                        case 1:
                        {
                            return ServerImpl::room_object2d_destroy(args);
                        }

                        // Move object
                        case 2:
                        {
                            return ServerImpl::room_object2d_move(args);
                        }

                        // Create moving object
                        case 3:
                        {
                            return ServerImpl::room_object2d_createMoving(args);
                        }
                    }
                    return CommandResult::not_found;
                }
                // Player3D
                case 4:
                {
                    switch (arg3)
                    {
                        /// Position 3D
                        case 0:
                        {
                            return ServerImpl::room_player3d_position(args);
                        }

                        // Dimension 3D
                        case 1:
                        {
                            return ServerImpl::room_player3d_dimension(args);
                        }

                        // Movement 3D
                        case 2:
                        {
                            return ServerImpl::room_player3d_movement(args);
                        }

                        // Shoot
                        case 3:
                        {
                            return ServerImpl::room_player3d_shoot(args);
                        }

                        // Set team
                        case 5:
                        {
                            return ServerImpl::room_player3d_setTeam(args);
                        }

                        // Positional audio event (sound at a world position).
                        case 7:
                        {
                            return ServerImpl::room_player3d_audioEvent(args);
                        }

                        // Game-defined round action: plant, defuse, or cancel.
                        case 8:
                        {
                            auto* room = RoomStorage::getRoomById(user.getRoomId());
                            if (!room || args.getData().size() != 4)
                                return CommandResult::invalid_input;
                            return room->onPlayerAction(user.getId(), args.getData()[3])
                                           ? CommandResult::success
                                           : CommandResult::invalid_input;
                        }
                    }
                    return CommandResult::not_found;
                }
                // Object3D
                case 5:
                {
                    switch (arg3)
                    {
                        case 0:
                        {
                            return ServerImpl::room_object3d_create(args);
                        }

                        case 1:
                        {
                            return ServerImpl::room_object3d_destroy(args);
                        }

                        case 2:
                        {
                            return ServerImpl::room_object3d_move(args);
                        }

                        case 3:
                        {
                            return ServerImpl::room_object3d_createMoving(args);
                        }
                    }
                    break;
                }

                // GameItem
                case 6:
                {
                    switch (arg3)
                    {
                        case 0:
                        {
                            return ServerImpl::room_gameitem_create(args);
                        }

                        case 1:
                        {
                            return ServerImpl::room_gameitem_update(args);
                        }

                        case 2:
                        {
                            return ServerImpl::room_gameitem_destroy(args);
                        }
                    }
                }
            }
            return CommandResult::not_found;
        }

        case CommandType::authentication:
        {
            switch (arg2)
            {
                // Setup authentication.
                case 0:
                {
                    Log::info("New client wants to authenticate, not implemented");
                    return CommandResult::not_found;
                }

                // Root access is available only on an authenticated TLS-PSK
                // Boost TCP session. Failed attempts are shared by source
                // address across reconnects to this server instance.
                case 1:
                {
                    if (m_settings.getMode() != AuthenticationMode::password_protected ||
                        !m_settings.isTlsEnabled() ||
                        protocol.getType() != NXProtocol::Type::BOOST_TCP_SERVER ||
                        !user.hasCommonAccess() || !m_settings.hasRootPassword())
                    {
                        return CommandResult::unauthorized;
                    }

                    if (user.hasRootAccess())
                    {
                        return CommandResult::success;
                    }

                    const auto now = std::chrono::steady_clock::now();
                    std::lock_guard lock(m_rootAuthState->mutex);
                    auto& attempts = m_rootAuthState->byAddress;
                    auto it = attempts.find(user.getIPAddress());
                    if (it != attempts.end() && now - it->second.firstFailure >= kRootAuthWindow)
                    {
                        it = attempts.erase(it);
                    }
                    if (it != attempts.end() && it->second.failures >= kMaxRootAuthFailures)
                    {
                        return CommandResult::unauthorized;
                    }
                    if (it == attempts.end() && attempts.size() >= kMaxTrackedRootAuthAddresses)
                    {
                        std::erase_if(attempts, [now](const auto& entry)
                                      { return now - entry.second.firstFailure >= kRootAuthWindow; });
                        if (attempts.size() >= kMaxTrackedRootAuthAddresses)
                        {
                            return CommandResult::unauthorized;
                        }
                    }

                    auto payload = Util::removeAmountOfBytesFromVector(command, 2);
                    const std::string password = Util::convertToString(payload);
                    if (m_settings.isRootPassword(password))
                    {
                        if (it != attempts.end())
                        {
                            attempts.erase(it);
                        }
                        user.setRootAccess(true);
                        Log::info("Client ", user.getIPAddress(), " has root access!");
                        return CommandResult::success;
                    }

                    if (it != attempts.end())
                    {
                        ++it->second.failures;
                    }
                    else
                    {
                        attempts.emplace(user.getIPAddress(), RootAuthState::Attempt{now, 1});
                    }
                    return CommandResult::unauthorized;
                }

                // Passphrase authentication belongs to the initial handshake,
                // not a command on an established session.
                case 2:
                {
                    return CommandResult::unauthorized;
                }
            }
            return CommandResult::not_found;
        }

        case CommandType::server_management:
        {
            return CommandResult::not_found;
        }

        case CommandType::player_management:
        {
            return CommandResult::not_found;
        }

        case CommandType::error:
        {
            // Internal server error
            switch (arg2)
            {
                // Classname X
                case 0:
                {
                    Log::critical("Internal server error: x");
                    return CommandResult::error;
                }
            }
            return CommandResult::not_found;
        }

        case CommandType::undefined:
        {
            return CommandResult::error;
        }
    }
    return CommandResult::not_found;
}

bool Command::sendMessageToClient(nx_data data, User& user, NXProtocol& protocol)
{
    auto trySend = [&data, &user](NXProtocol::Type type) -> bool
    {
        switch (type)
        {
            case NXProtocol::Type::BOOST_TCP_SERVER:
                return user.boostTCPSend(data);
            case NXProtocol::Type::BOOST_UDP_SERVER:
                return user.boostUDPSend(data);
            case NXProtocol::Type::AF_UNIX_SOCK_STREAM_SERVER:
                return user.unixStreamSend(data);
            case NXProtocol::Type::AF_UNIX_SOCK_DGRAM_SERVER:
                return user.unixDgramSend(data);
            case NXProtocol::Type::AF_INET_TCP_SERVER:
                return user.inetTCPSend(data);
            case NXProtocol::Type::AF_INET_UDP_SERVER:
                return user.inetUDPSend(data);
            default:
                return false;
        }
    };

    if (trySend(protocol.getType()))
    {
        return true;
    }

    static const NXProtocol::Type allServerProtocols[] = {
            NXProtocol::Type::BOOST_TCP_SERVER,
            NXProtocol::Type::BOOST_UDP_SERVER,
            NXProtocol::Type::AF_UNIX_SOCK_STREAM_SERVER,
            NXProtocol::Type::AF_UNIX_SOCK_DGRAM_SERVER,
            NXProtocol::Type::AF_INET_TCP_SERVER,
            NXProtocol::Type::AF_INET_UDP_SERVER,
    };

    for (auto fallbackType : allServerProtocols)
    {
        if (fallbackType == protocol.getType())
            continue;
        if (trySend(fallbackType))
            return true;
    }

    Log::error("Failed to send message to client via any protocol");
    return false;
}

std::string roomActionToString(RoomCommandType::Root root, uint8_t action)
{
    switch (root)
    {
        case RoomCommandType::Root::management:
            return RoomCommandType::ManagementTypeToString(
                    static_cast<RoomCommandType::Management>(action));

        case RoomCommandType::Root::player_2D:
        case RoomCommandType::Root::player_3D:
            return RoomCommandType::PlayerTypeToString(
                    static_cast<RoomCommandType::PlayerType>(action));

        case RoomCommandType::Root::object_2D:
        case RoomCommandType::Root::object_3D:
            return RoomCommandType::ObjectTypeToString(
                    static_cast<RoomCommandType::ObjectType>(action));

        case RoomCommandType::Root::game_item:
            return RoomCommandType::GameItemActionToString(
                    static_cast<RoomCommandType::GameItemAction>(action));

        case RoomCommandType::Root::communication:
            return RoomCommandType::CommunicationTypeToString(
                    static_cast<RoomCommandType::Communication>(action));

        case RoomCommandType::Root::undefined:
        default:
            Log::error("createRoomCommand: Undefined roomType or unsupported action");
            return {};
    }
}

nx_data Command::createRoomCommand(uint64_t roomId, User& user, const nx_data& messageData, const std::map<std::string, boost::json::value>& params, uint64_t messageId)
{
    if (messageData.size() < 3)
    {
        Log::error("createRoomCommand: Insufficient messageData");
        return nx_data();
    }

    auto room_type = static_cast<RoomCommandType::Root>(messageData[1]);
    auto action = messageData[2];

    std::string room_command_action = roomActionToString(room_type, action);
    if (room_command_action.empty())
    {
        Log::error("createRoomCommand: Failed to convert room action to string");
        return nx_data();
    }

    std::string room_command_type = RoomCommandType::RoomTypeToString(room_type);
    if (room_command_type.empty())
    {
        Log::error("createRoomCommand: Failed to convert roomType");
        return nx_data();
    }

    auto all_params = ClientMsgType{
            {"action", boost::json::value(room_command_action)},
            {"room_id", boost::json::value(roomId)},
            {"client_id", boost::json::value(user.getId())},
    };
    all_params.insert(params.begin(), params.end());

    return clientMessageData(CommandType::room, room_command_type, messageId, all_params);
}

boost::json::array Command::playerStatsEntries(const Room& room, const std::vector<uint64_t>& clientIds)
{
    boost::json::array entries;
    for (auto clientId : clientIds)
    {
        auto* client = ClientStorage::getClientById(clientId);
        if (!client)
            continue;

        boost::json::object entry;
        entry["id"] = boost::json::value(clientId);
        entry["username"] = boost::json::value(client->getUsername());
        entry["team"] = boost::json::value(room.getPlayerTeam(clientId));
        entry["kills"] = boost::json::value(room.getPlayerKills(clientId));
        entry["deaths"] = boost::json::value(room.getPlayerDeaths(clientId));
        entries.emplace_back(std::move(entry));
    }
    return entries;
}

nx_data Command::createRoomLeaderboardCommand(uint64_t roomId, User& user,
                                              boost::json::array entries, uint64_t messageId)
{
    std::map<std::string, boost::json::value> leaderboardParams{
            {"entries", boost::json::value(std::move(entries))}};

    nx_data leaderboardData;
    leaderboardData.emplace_back(static_cast<uint8_t>(CommandType::room));
    leaderboardData.emplace_back(static_cast<uint8_t>(RoomCommandType::Root::player_3D));
    leaderboardData.emplace_back(static_cast<uint8_t>(RoomCommandType::PlayerType::leaderboard));

    return Command::createRoomCommand(roomId, user, leaderboardData, leaderboardParams, messageId);
}

bool Command::sendRoomCommand(const nx_data& data, User& user, NXProtocol& protocol)
{
    auto& rooms = RoomStorage::getAllRooms();
    for (auto& room : rooms)
    {
        if (room.getId() == user.getRoomId())
        {
            bool all_success = true;
            for (auto& roomClient : room.getClients())
            {
                auto* client = ClientStorage::getClientById(roomClient);
                if (!sendMessageToClient(data, *client, protocol))
                    all_success = false;
            }
            return all_success;
        }
    }
    return false;
}

void Command::runWithTickrate(double tickrate, double durationSeconds, const std::function<void(double)>& tickFunction)
{
    using namespace std::chrono;

    auto interval = duration_cast<milliseconds>(milliseconds(static_cast<int>(1000 / tickrate)));
    auto startTime = steady_clock::now();
    auto endTime = startTime + duration_cast<milliseconds>(milliseconds(static_cast<int>(durationSeconds * 1000)));

    double totalTicks = tickrate * durationSeconds;

    for (int tick = 1; steady_clock::now() <= endTime && tick <= totalTicks; ++tick)
    {
        auto loopStart = steady_clock::now();
        double progress = static_cast<double>(tick) / totalTicks;
        tickFunction(progress);
        std::this_thread::sleep_until(loopStart + interval);
    }
}

nx_data Command::clientMessageData(CommandType cmd, const std::string& type, uint64_t message_id, const ClientMsgType& params)
{
    auto vector = Util::convertToByteVector(Json::createJSON(clientMessageMap(cmd, type, message_id, params)));
    vector.emplace_back('\n');
    return vector;
}

Command::ClientMsgType Command::clientMessageMap(CommandType cmd, const std::string& type, uint64_t message_id, const ClientMsgType& params)
{
    auto data = ClientMsgType{
            {"command", boost::json::value(commandTypeAsString(cmd))},
            {"type", boost::json::value(type)},
            {"callback", boost::json::value(message_id)}};
    data.insert(params.begin(), params.end());
    return data;
}

} // namespace nexilis::server
