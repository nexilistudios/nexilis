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

#ifndef NEXILIS_CLIENT_COMMAND_GET_INFO_ROOMS_HH
#define NEXILIS_CLIENT_COMMAND_GET_INFO_ROOMS_HH

#include <nexilis/client/base_api_command.hh>

namespace nexilis::client
{

class GetInfoRoomsCommand : public BaseAPICommand
{
public:
    explicit GetInfoRoomsCommand(boost::json::value rooms)
        : m_rooms(std::move(rooms))
    {
    }

    ReadResult execute(ClientAPI& api, ClientAPI::ClientAPIData& data) override
    {
        if (!m_rooms.is_array())
        {
            return ReadResult::invalid_input;
        }

        auto rooms = m_rooms.as_array();
        std::vector<Room> newRooms;
        for (const auto& room : rooms)
        {
            std::string name;
            if (room.at("name").is_string())
                name = room.at("name").as_string().c_str();

            uint64_t maxSize = Util::toUint64(room.at("max_size"));
            uint64_t creatorId = Util::toUint64(room.at("creator_id"));
            uint64_t id = Util::toUint64(room.at("room_id"));
            auto ctx = static_cast<RoomData::Context>(Util::toUint64(room.at("context")));

            std::vector<ClientSession> roomClients;

            if (room.as_object().contains("clients"))
            {
                auto clients = room.at("clients").as_array();

                for (const auto& client : clients)
                {
                    uint64_t client_id = Util::toUint64(client.at("id"));

                    std::string username;
                    if (client.at("name").is_string())
                        username = client.at("name").as_string().c_str();

                    float object2DX = 0.0f;
                    if (client.at("x").is_double())
                        object2DX = static_cast<float>(client.at("x").as_double());

                    float object2DY = 0.0f;
                    if (client.at("y").is_double())
                        object2DY = static_cast<float>(client.at("y").as_double());

                    float dimension2DX = 0.0f;
                    if (client.at("width").is_double())
                        dimension2DX = static_cast<float>(client.at("width").as_double());

                    float dimension2DY = 0.0f;
                    if (client.at("height").is_double())
                        dimension2DY = static_cast<float>(client.at("height").as_double());

                    ClientSession newClient(client_id, &api);
                    newClient.getObject2D().setPosition({object2DX, object2DY});
                    newClient.getObject2D().setDimensions({dimension2DX, dimension2DY});
                    newClient.setUsername(username);
                    roomClients.emplace_back(std::move(newClient));
                }
            }

            auto roomData = RoomData(creatorId, name, id, ctx, maxSize);
            newRooms.emplace_back(Room(roomData, std::move(roomClients)));
            if (const auto* teams = room.as_object().if_contains("available_teams"))
            {
                if (!teams->is_array())
                    return ReadResult::invalid_input;
                std::vector<std::string> names;
                for (const auto& team : teams->as_array())
                {
                    if (!team.is_string())
                        return ReadResult::invalid_input;
                    names.emplace_back(team.as_string().c_str());
                }
                newRooms.back().setAvailableTeams(std::move(names));
            }

            // 2D context room cannot have 3D objects.
            if (ctx != RoomData::Context::_2D)
            {
                // Parse 3D objects from the room info.
                if (room.as_object().contains("objects_3d"))
                {
                    auto& roomRef = newRooms.back();
                    auto objects = room.at("objects_3d").as_array();
                    for (const auto& obj : objects)
                    {
                        uint64_t obj_id = Util::toUint64(obj.at("id"));
                        float x = static_cast<float>(obj.at("x").as_double());
                        float y = static_cast<float>(obj.at("y").as_double());
                        float z = static_cast<float>(obj.at("z").as_double());
                        float w = static_cast<float>(obj.at("w").as_double());
                        float h = static_cast<float>(obj.at("h").as_double());
                        float d = static_cast<float>(obj.at("d").as_double());
                        std::string filepath;
                        if (obj.as_object().contains("filepath"))
                            filepath = obj.at("filepath").as_string().c_str();

                        auto object3D = Object3D(obj_id, {x, y, z}, {w, h, d});
                        object3D.setFilepath(filepath);
                        roomRef.addObject(std::move(object3D));
                    }
                }
            }

            // Parse 2D objects, they are allowed to exist in all contexts.
            if (room.as_object().contains("objects_2d"))
            {
                auto& roomRef = newRooms.back();
                auto objects = room.at("objects_2d").as_array();
                for (const auto& obj : objects)
                {
                    uint64_t obj_id = Util::toUint64(obj.at("id"));
                    float x = static_cast<float>(obj.at("x").as_double());
                    float y = static_cast<float>(obj.at("y").as_double());
                    float w = static_cast<float>(obj.at("w").as_double());
                    float h = static_cast<float>(obj.at("h").as_double());
                    std::string filepath;
                    if (obj.as_object().contains("filepath"))
                        filepath = obj.at("filepath").as_string().c_str();

                    auto object2D = Object2D(obj_id, {x, y}, {w, h});
                    object2D.setFilepath(filepath);
                    roomRef.addObject(std::move(object2D));
                }
            }
        }

        auto& mtx = data.getRoomsMutex();
        if (mtx)
        {
            std::lock_guard<std::mutex> lock(*mtx);
            data.setCurrentlyActiveRooms(std::move(newRooms));
        }

        return ReadResult::success;
    }

private:
    boost::json::value m_rooms;
};

} // namespace nexilis::client

#endif
