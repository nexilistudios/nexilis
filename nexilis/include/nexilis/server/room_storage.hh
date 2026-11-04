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

#ifndef NEXILIS_ROOM_STORAGE_HH
#define NEXILIS_ROOM_STORAGE_HH

#include <nexilis/server/room.hh>
#include <nexilis/server/user.hh>

#include <functional>
#include <vector>

namespace nexilis::server
{

/// Creating static lifetime for the rooms in the server context.
class RoomStorage
{
public:
    /// Constructor.
    RoomStorage() = default;

    /// Add new room to the server.
    static void add(Room&& room);
    /// Called for each room after it is stored, before creation is acknowledged.
    static void setCreateHandler(std::function<void(Room&)> handler);

    /// Check if room exists.
    /// \param id The id of the room.
    static bool contains(uint64_t id);

    /// Get all the rooms in the server.
    static std::vector<Room>& getAllRooms();

    /// Get pointer of the room.
    /// \param id The id of the room.
    static Room* getRoomById(uint64_t id);

    static void clear();

private:
    static std::vector<Room> m_rooms;
    static std::function<void(Room&)> m_createHandler;
};

} // namespace nexilis::server

#endif
