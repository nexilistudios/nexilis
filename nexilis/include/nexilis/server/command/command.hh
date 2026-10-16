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

#ifndef NEXILIS_COMMAND_HH
#define NEXILIS_COMMAND_HH

#include <nexilis/command_type.hh>
#include <nexilis/movement/movement_2D.hh>
#include <nexilis/movement/movement_3D.hh>
#include <nexilis/nx_class.hh>
#include <nexilis/nx_protocol.hh>
#include <nexilis/server/command/command_result.hh>
#include <nexilis/server/room.hh>
#include <nexilis/server/server_config.hh>
#include <nexilis/server/user.hh>

#include <boost/json/array.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace nexilis::server
{

/// Nexilis Server-side API.
/// Command contains functionality for reading nexilis byte sequence.
/// These bytes have been cleared from MessageHandler and contains vector<uint8>& which triggers all the actions of nexilis.
class Command : public NxClass
{
public:
    using ClientMsgType = std::map<std::string, boost::json::value>;

    /// Constructor.
    /// \param settings The settings of the server.
    explicit Command(const ServerConfig& settings);

    // Move constructor.
    Command(Command&& other);

    // Move assignment operator.
    Command& operator=(Command&& other);

    /// Deleted copy constructor.
    Command(const Command& other) = delete;

    /// Deleted copy assignment operator.
    Command& operator=(const Command& other) = delete;

    /// Read the command from client.
    /// \param command The vector of bytes that is the command.
    /// \param user The user that sent the message.
    /// \param protocol The protocol that was used in receiving the message.
    /// \param messageId The unique identifier for the message.
    /// \return Result from reading the command.
    CommandResult read(const nx_data& command, User& user, NXProtocol& protocol, uint64_t messageId);

    /// Read the command from client.
    /// \param command_data The data for the command
    /// \param length The command length in bytes.
    /// \param user The user that sent the message.
    /// \param protocol The protocol that was used in receiving the message.
    /// \param messageId The unique identifier for the message.
    /// \return Result from reading the command.
    CommandResult read(const char* command_data, size_t length, User& client, NXProtocol& protocol, uint64_t messageId);

    ServerConfig& getServerConfig()
    {
        return m_settings;
    }

    const ServerConfig& getServerConfig() const
    {
        return m_settings;
    }

    /// Convert ClientMsgType to nx_data (and add a '\n') as a last character.
    static nx_data clientMessageData(CommandType cmd, const std::string& type, uint64_t message_id, const ClientMsgType& params);

    /// Send message to every protocol that is avainable for a client.
    static bool sendMessageToClient(nx_data data, User& user, NXProtocol& protocol);

    /// Send message in room context.
    static bool sendRoomCommand(const nx_data& data, User& user, NXProtocol& protocol);

    /// Create a serverside package for commands happening inside a room.
    /// This function is expected to be called by ServerImpl room functions.
    static nx_data createRoomCommand(uint64_t roomId, User& user, const nx_data& messageData, const ClientMsgType& params, uint64_t messageId);

    /// Build the "entries" array of a player stats payload: one object per
    /// requested client id with that player's current username, team, kills and
    /// deaths as tracked by the room. Clients merge these into their local
    /// kill/death table rather than asking for it.
    static boost::json::array playerStatsEntries(const Room& room, const std::vector<uint64_t>& clientIds);

    /// Wrap player stats entries into a room "player_3D" leaderboard command
    /// that clients parse as a stats update to merge into their local table.
    /// Callers decide whether to broadcast (e.g. on kill) or unicast (e.g. to
    /// seed a fresh joiner) the returned packet.
    static nx_data createRoomLeaderboardCommand(uint64_t roomId, User& user,
                                                boost::json::array entries, uint64_t messageId);

    /// Send multiple messages with specified tickrate.
    static void runWithTickrate(double tickrate, double durationSeconds, const std::function<void(double)>& tickFunction);

private:
    /// Add fields "command", "type" and "callback" to existing ClientMsgType.
    static ClientMsgType clientMessageMap(CommandType cmd, const std::string& type, uint64_t message_id, const ClientMsgType& params);

private:
    struct RootAuthState;

    /// The server side configuration.
    ServerConfig m_settings;
    std::shared_ptr<RootAuthState> m_rootAuthState;
};

} // namespace nexilis::server

#endif
