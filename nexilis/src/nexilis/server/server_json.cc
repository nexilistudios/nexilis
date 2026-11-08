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
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/room_storage.hh>
#include <nexilis/server/server_json.hh>

#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>

namespace nexilis::server
{

boost::json::object ServerJson::getRoomData()
{
    boost::json::object roomDataObj;

    auto rooms = RoomStorage::getAllRooms();

    if (rooms.empty())
    {
        Log::info("Empty room data");
        roomDataObj["room_amount"] = 0;
    }
    else
    {
        roomDataObj["room_amount"] = rooms.size();
        auto roomData = roomsToJSON(rooms);
        roomDataObj["rooms"] = std::move(roomData);
    }
    return roomDataObj;
}

boost::json::object ServerJson::getClientData()
{
    boost::json::object clientDataObj;

    auto clients = ClientStorage::getAllClients();

    if (clients.empty())
    {
        Log::info("Empty client data");
        clientDataObj["client_amount"] = 0;
    }
    else
    {
        clientDataObj["client_amount"] = clients.size();
        auto clientData = clientsToJSON(clients);
        clientDataObj["clients"] = std::move(clientData);
    }
    return clientDataObj;
}

boost::json::object ServerJson::getServerData()
{
    boost::json::object serverDataObj;

    Json::emplace(serverDataObj, getClientData());
    Json::emplace(serverDataObj, getRoomData());

    return serverDataObj;
}

boost::json::array ServerJson::clientsToJSON(const std::vector<std::shared_ptr<User>>& clients)
{
    boost::json::array resultingArray;

    for (const auto& client : clients)
    {
        boost::json::object clientObj;
        if (client->getUsername().empty())
        {
            clientObj["username"] = "UNIDENTIFIED_CLIENT";
        }
        else
        {
            clientObj["username"] = client->getUsername();
        }
        clientObj["id"] = client->getId();
        clientObj["room_id"] = client->getRoomId();
        resultingArray.emplace_back(std::move(clientObj));
    }
    return resultingArray;
}

boost::json::array ServerJson::roomsToJSON(const std::vector<std::shared_ptr<Room>>& rooms)
{
    boost::json::array resultingArray;
    resultingArray.reserve(rooms.size());

    for (const auto& room : rooms)
    {
        boost::json::object roomObj;

        if (room->getName().empty())
        {
            roomObj["name"] = "UNIDENTIFIED_ROOM";
        }
        else
        {
            roomObj["name"] = room->getName();
        }

        roomObj["max_size"] = room->getMaxSize();
        roomObj["room_id"] = room->getId();
        roomObj["creator_id"] = room->getCreatorId();
        auto ctx = room->getContext();
        roomObj["context"] = static_cast<uint32_t>(ctx);

        // Get data from clients in a room.
        boost::json::array clientArray;
        clientArray.reserve(room->getClients().size());

        auto& roomClients = room->getClients();

        for (auto& client : roomClients)
        {
            auto clientPointer = ClientStorage::getClientById(client);

            if (clientPointer)
            {
                boost::json::object clientObj;
                clientObj["id"] = clientPointer->getId();

                if (clientPointer->getUsername().empty())
                {
                    Log::warning("Client has empty username!");
                    clientObj["name"] = "";
                }
                else
                {
                    clientObj["name"] = clientPointer->getUsername();
                }

                auto position2D = clientPointer->getObject2D().getPosition();
                clientObj["x"] = std::move(position2D.x);
                clientObj["y"] = std::move(position2D.y);

                auto dimension2D = clientPointer->getObject2D().getDimensions();
                clientObj["width"] = std::move(dimension2D.x);
                clientObj["height"] = std::move(dimension2D.y);

                clientArray.emplace_back(std::move(clientObj));
            }
        }

        if (roomClients.size() > 0)
        {
            roomObj["clients"] = std::move(clientArray);
        }

        // 2D context cannot have 3D objects.
        if (ctx != RoomData::Context::_2D)
        {
            if (!room->getObjects3D().empty())
            {
                boost::json::array obj3DArray;
                for (const auto& obj : room->getObjects3D())
                {
                    auto pos = obj.getPosition();
                    auto dim = obj.getDimensions();
                    boost::json::object objData;
                    objData["id"] = obj.getId();
                    objData["x"] = static_cast<double>(pos.x);
                    objData["y"] = static_cast<double>(pos.y);
                    objData["z"] = static_cast<double>(pos.z);
                    objData["w"] = static_cast<double>(dim.x);
                    objData["h"] = static_cast<double>(dim.y);
                    objData["d"] = static_cast<double>(dim.z);
                    objData["filepath"] = obj.getFilepath();
                    obj3DArray.emplace_back(std::move(objData));
                }
                roomObj["objects_3d"] = std::move(obj3DArray);
            }
        }

        // 3D context can have 2D objects, they just don't have depth.
        if (!room->getObjects2D().empty())
        {
            boost::json::array obj2DArray;
            for (const auto& obj : room->getObjects2D())
            {
                auto pos = obj.getPosition();
                auto dim = obj.getDimensions();
                boost::json::object objData;
                objData["id"] = obj.getId();
                objData["x"] = static_cast<double>(pos.x);
                objData["y"] = static_cast<double>(pos.y);
                objData["w"] = static_cast<double>(dim.x);
                objData["h"] = static_cast<double>(dim.y);
                objData["filepath"] = obj.getFilepath();
                obj2DArray.emplace_back(std::move(objData));
            }
            roomObj["objects_2d"] = std::move(obj2DArray);
        }

        resultingArray.emplace_back(std::move(roomObj));
    }
    return resultingArray;
}

} // namespace nexilis::server
