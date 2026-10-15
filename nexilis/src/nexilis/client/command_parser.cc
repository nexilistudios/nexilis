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

#include <nexilis/client/command/get/client_id.hh>
#include <nexilis/client/command/get/info_client_count.hh>
#include <nexilis/client/command/get/info_rooms.hh>
#include <nexilis/client/command/get/room_id.hh>
#include <nexilis/client/command/room/communication.hh>
#include <nexilis/client/command/room/gameitem.hh>
#include <nexilis/client/command/room/leaderboard.hh>
#include <nexilis/client/command/room/management.hh>
#include <nexilis/client/command/room/match.hh>
#include <nexilis/client/command/room/object2d.hh>
#include <nexilis/client/command/room/object3d.hh>
#include <nexilis/client/command/room/player2d.hh>
#include <nexilis/client/command/room/player3d.hh>
#include <nexilis/client/command/set/port.hh>
#include <nexilis/client/command/set/username.hh>
#include <nexilis/client/command_parser.hh>

#include <boost/json/array.hpp>
#include <boost/json/value.hpp>

#include <memory>
#include <string_view>
#include <utility>

namespace nexilis::client
{

template <typename... Args>
std::unique_ptr<ErrorCommand<Args...>> makeError(ReadResult result, Args&&... args)
{
    return std::make_unique<ErrorCommand<Args...>>(result, std::forward<Args>(args)...);
}

uint64_t getUint64(const boost::json::object& json, std::string_view key)
{
    return json.contains(key) ? Util::toUint64(json.at(key)) : 0;
}

float getFloat(const boost::json::object& json, std::string_view key)
{
    return json.contains(key) ? static_cast<float>(json.at(key).as_double()) : 0.0f;
}

std::string getString(const boost::json::object& json, std::string_view key)
{
    return json.contains(key) ? json.at(key).as_string().c_str() : "";
}

std::unique_ptr<BaseAPICommand> CommandParser::parse(const boost::json::object& json)
{
    if (!json.contains("command") || !json.contains("type"))
    {
        return makeError(ReadResult::parsing_failed, "no \"command\" or \"type\" found!");
    }

    auto cmd_type = commandTypeFromString(json.at("command").as_string().c_str());
    auto type = json.at("type").as_string();

    switch (cmd_type)
    {
        case CommandType::setting:
            return parseSettingCommand(json, type);
        case CommandType::getting:
            return parseGettingCommand(json, type);
        case CommandType::room:
            return parseRoomCommand(json, type);
        case CommandType::error:
            return parseErrorCommand(type);
        case CommandType::authentication:
        default:
            return makeError(ReadResult::parsing_failed, "command parsing failed to execute");
    }
    return makeError(ReadResult::parsing_failed, "command type not found");
}

std::unique_ptr<BaseAPICommand> CommandParser::parseSettingCommand(const boost::json::object& json, std::string_view type)
{
    if (type == "username")
    {
        std::string username = json.at("username").as_string().c_str();

        uint64_t client_id = 0;
        if (json.contains("client_id"))
            client_id = Util::toUint64(json.at("client_id"));

        return std::make_unique<SetUsernameCommand>(client_id, username);
    }
    else if (type == "port")
    {
        std::string protocol = "boost_tcp";
        if (json.contains("protocol"))
            protocol = json.at("protocol").as_string().c_str();

        uint16_t port = 0;
        if (json.contains("port"))
            port = static_cast<uint16_t>(Util::toUint64(json.at("port")));
        else if (json.contains("boost_tcp_port"))
            port = static_cast<uint16_t>(Util::toUint64(json.at("boost_tcp_port")));

        return std::make_unique<SetPortCommand>(protocol, port);
    }
    return makeError(ReadResult::parsing_failed, "cannot find set command type");
}

std::unique_ptr<BaseAPICommand> CommandParser::parseGettingCommand(const boost::json::object& json, std::string_view type)
{
    if (type == "client_id")
    {
        uint64_t client_id = Util::toUint64(json.at("client_id"));
        return std::make_unique<GetClientIdCommand>(client_id);
    }

    else if (type == "room_id")
    {
        uint64_t room_id = Util::toUint64(json.at("room_id"));
        return std::make_unique<GetRoomIdCommand>(room_id);
    }

    else if (type == "info_rooms")
    {
        if (json.contains("rooms"))
        {
            return std::make_unique<GetInfoRoomsCommand>(json.at("rooms"));
        }

        return makeError(ReadResult::invalid_input);
    }

    else if (type == "info_general" && json.contains("client_amount"))
    {
        return std::make_unique<GetInfoClientCountCommand>(
                Util::toUint64(json.at("client_amount")));
    }

    return makeError(ReadResult::parsing_failed, "cannot find get command type");
}

std::unique_ptr<BaseAPICommand> CommandParser::parseRoomCommand(const boost::json::object& json, std::string_view type)
{
    // Parse room commands based on 'type' and 'action' fields
    if (!json.contains("action"))
    {
        return makeError(ReadResult::parsing_failed, "missing \"action\"");
    }

    std::string action = json.at("action").as_string().c_str();

    // Common fields for all room commands
    uint64_t room_id = getUint64(json, "room_id");
    uint64_t client_id = getUint64(json, "client_id");

    if (type == "match" && action == "state")
    {
        const std::string gameMode = getString(json, "game_mode");
        if (gameMode == "deathmatch")
        {
            auto event = std::make_unique<ClientAPI::DeathmatchMatchEvent>();
            event->room_id = room_id;
            event->player_id = getUint64(json, "player_id");
            event->alive = json.contains("alive") && json.at("alive").as_bool();
            event->respawn_seconds = getFloat(json, "respawn_seconds");
            return std::make_unique<RoomMatchCommand>(std::move(event));
        }
        if (gameMode.empty() || gameMode == "bomb")
        {
            auto event = std::make_unique<ClientAPI::BombMatchEvent>();
            event->room_id = room_id;
            event->round = getUint64(json, "round");
            event->terrorist_score = getUint64(json, "terrorist_score");
            event->counter_terrorist_score = getUint64(json, "counter_terrorist_score");
            event->queue_position = getUint64(json, "queue_position");
            event->phase = getString(json, "phase");
            event->team = getString(json, "team");
            event->notice = getString(json, "notice");
            event->seconds = getFloat(json, "seconds");
            event->bomb_x = getFloat(json, "bomb_x");
            event->bomb_y = getFloat(json, "bomb_y");
            event->bomb_z = getFloat(json, "bomb_z");
            event->bomb_planted = json.contains("bomb_planted") && json.at("bomb_planted").as_bool();
            event->active = json.contains("active") && json.at("active").as_bool();
            event->alive = json.contains("alive") && json.at("alive").as_bool();
            if (json.contains("alive_players") && json.at("alive_players").is_array())
                for (const auto& id : json.at("alive_players").as_array())
                    event->alive_players.push_back(Util::toUint64(id));
            return std::make_unique<RoomMatchCommand>(std::move(event));
        }
        return makeError(ReadResult::parsing_failed, "unknown match game mode");
    }

    if (type == "management")
    {
        return std::make_unique<RoomManagementCommand>(
                action, room_id, client_id,
                getString(json, "room_name"),
                getUint64(json, "room_context"),
                getString(json, "username"),
                json.contains("overlap_allowed") ? json.at("overlap_allowed").as_bool() : true);
    }
    else if (type == "player_2D")
    {
        if (action == "shoot")
        {
            return std::make_unique<RoomPlayer2DShootCommand>(
                    client_id,
                    getFloat(json, "x"), getFloat(json, "y"));
        }
        return std::make_unique<RoomPlayer2DCommand>(action, client_id,
                                                     getFloat(json, "x"), getFloat(json, "y"));
    }
    else if (type == "player_3D")
    {
        if (action == "leaderboard")
        {
            boost::json::array entries;
            if (json.contains("entries") && json.at("entries").is_array())
                entries = json.at("entries").as_array();
            return std::make_unique<RoomLeaderboardCommand>(entries);
        }

        if (action == "audio_event")
        {
            return std::make_unique<RoomAudioEventCommand>(
                    client_id, static_cast<uint8_t>(getUint64(json, "sound")),
                    getFloat(json, "x"), getFloat(json, "y"), getFloat(json, "z"));
        }

        if (action == "animation")
        {
            return std::make_unique<RoomAnimationEventCommand>(
                    room_id, client_id, getString(json, "state"),
                    getString(json, "weapon"), getFloat(json, "duration"));
        }

        return std::make_unique<RoomPlayer3DCommand>(
                action, client_id,
                getFloat(json, "x"), getFloat(json, "y"), getFloat(json, "z"),
                getUint64(json, "target_id"),
                getFloat(json, "damage"), getFloat(json, "new_health"),
                getFloat(json, "yaw"));
    }
    else if (type == "object_2D")
    {
        return std::make_unique<RoomObject2DCommand>(
                action, room_id,
                getUint64(json, "id"),
                getFloat(json, "x"), getFloat(json, "y"),
                getFloat(json, "width"), getFloat(json, "height"),
                getString(json, "filepath"),
                getString(json, "createMovingType"));
    }
    else if (type == "object_3D")
    {
        return std::make_unique<RoomObject3DCommand>(
                action, room_id,
                getUint64(json, "id"),
                getFloat(json, "x"), getFloat(json, "y"), getFloat(json, "z"),
                getFloat(json, "w"), getFloat(json, "h"), getFloat(json, "d"),
                getString(json, "filepath"),
                getString(json, "createMovingType"));
    }
    else if (type == "game_item")
    {
        return std::make_unique<RoomGameItemCommand>(
                action, room_id,
                getUint64(json, "id"),
                getFloat(json, "x"), getFloat(json, "y"), getFloat(json, "z"),
                getFloat(json, "w"), getFloat(json, "h"), getFloat(json, "d"),
                getString(json, "item_type"), getString(json, "status"),
                getString(json, "filepath"));
    }
    else if (type == "communication")
    {
        return std::make_unique<RoomCommunicationCommand>(action, room_id, client_id,
                                                          getString(json, "message"));
    }

    return makeError<>(ReadResult::parsing_failed, "cannot find room command type");
}

std::unique_ptr<BaseAPICommand> CommandParser::parseErrorCommand(std::string_view type)
{
    auto result = readResultFromString(type);
    return ErrorCommand<>::make_unique(result.value_or(ReadResult::parsing_failed));
}

} // namespace nexilis::client
