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

#include <nexilis/logger/log.hh>
#include <nexilis/server/room_storage.hh>

namespace nexilis::server
{

std::vector<Room> RoomStorage::m_rooms = {};
std::function<void(Room&)> RoomStorage::m_createHandler;

void RoomStorage::setCreateHandler(std::function<void(Room&)> handler)
{
    m_createHandler = std::move(handler);
}

std::vector<Room>& RoomStorage::getAllRooms()
{
    return m_rooms;
}

void RoomStorage::add(Room&& room)
{
    Log::info("New room: ", room.getName(), " id: ", room.getId());
    m_rooms.emplace_back(std::move(room));
    if (m_createHandler)
        m_createHandler(m_rooms.back());
    Log::info("Total room amount = ", m_rooms.size());
}

bool RoomStorage::contains(uint64_t id)
{
    return std::find_if(m_rooms.begin(), m_rooms.end(),
                        [id](const Room& room)
                        {
                            return room.getId() == id;
                        }) != m_rooms.end();
}

Room* RoomStorage::getRoomById(uint64_t id)
{
    auto it = std::find_if(m_rooms.begin(), m_rooms.end(),
                           [id](const Room& room)
                           {
                               return room.getId() == id;
                           });

    if (it != m_rooms.end())
    {
        return &(*it);
    }
    else
    {
        return nullptr;
    }
}

void RoomStorage::clear()
{
    m_rooms.clear();
}

} // namespace nexilis::server
