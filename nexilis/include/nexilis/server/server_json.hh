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

#ifndef NEXILIS_SERVER_JSON_HH
#define NEXILIS_SERVER_JSON_HH

#include <nexilis/server/room.hh>
#include <nexilis/server/user.hh>

#include <boost/json/object.hpp>

#include <memory>
#include <vector>

namespace nexilis::server
{
/// Boost abstraction for serverside clients and rooms.
class ServerJson
{
public:
    /// Default constructor.
    ServerJson() = default;

    /// Get data about the rooms in the server.
    static boost::json::object getRoomData();

    /// Get data about the clients in the server.
    static boost::json::object getClientData();

    /// Return all data from the server.
    static boost::json::object getServerData();

private:
    // Get json data from room vector.
    static boost::json::array roomsToJSON(const std::vector<std::shared_ptr<Room>>& rooms);

    /// Get json data from client vector.
    static boost::json::array clientsToJSON(const std::vector<std::shared_ptr<User>>& clients);
};

} // namespace nexilis::server

#endif
