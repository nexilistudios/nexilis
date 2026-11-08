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

#include <algorithm>

namespace nexilis::server
{

std::vector<std::shared_ptr<Room>> RoomStorage::m_rooms;
std::mutex RoomStorage::m_mutex;
std::function<void(Room&)> RoomStorage::m_createHandler;

void RoomStorage::setCreateHandler(std::function<void(Room&)> handler)
{
    // Destroy the old callable outside the storage lock.
    {
        std::lock_guard lock(m_mutex);
        m_createHandler.swap(handler);
    }
}

std::vector<std::shared_ptr<Room>> RoomStorage::getAllRooms()
{
    std::lock_guard lock(m_mutex);
    return m_rooms;
}

bool RoomStorage::add(Room&& room, std::size_t maxRooms, std::size_t maxRoomsPerCreator)
{
    std::shared_ptr<Room> stored;
    std::function<void(Room&)> handler;
    {
        std::lock_guard lock(m_mutex);
        if (m_rooms.size() >= maxRooms ||
            std::any_of(m_rooms.begin(), m_rooms.end(), [&](const auto& existing)
                        { return existing->getId() == room.getId(); }) ||
            static_cast<std::size_t>(std::count_if(m_rooms.begin(), m_rooms.end(),
                                                   [&](const auto& existing)
                                                   { return existing->getCreatorId() == room.getCreatorId(); })) >= maxRoomsPerCreator)
            return false;
        handler = m_createHandler;
        stored = std::make_shared<Room>(std::move(room));
        m_rooms.emplace_back(stored);
    }
    // Callbacks may re-enter storage and may remove this room. Keep ownership
    // until the callback returns and never call application code under the lock.
    if (handler)
        handler(*stored);
    return true;
}

bool RoomStorage::contains(uint64_t id)
{
    return getRoomById(id) != nullptr;
}

std::size_t RoomStorage::getRoomCount()
{
    std::lock_guard lock(m_mutex);
    return m_rooms.size();
}

std::size_t RoomStorage::getRoomCountByCreator(uint64_t creatorId)
{
    std::lock_guard lock(m_mutex);
    return static_cast<std::size_t>(std::count_if(
            m_rooms.begin(), m_rooms.end(), [creatorId](const auto& room)
            { return room->getCreatorId() == creatorId; }));
}

std::shared_ptr<Room> RoomStorage::getRoomById(uint64_t id)
{
    std::lock_guard lock(m_mutex);
    auto it = std::find_if(m_rooms.begin(), m_rooms.end(),
                           [id](const auto& room)
                           { return room->getId() == id; });
    return it == m_rooms.end() ? nullptr : *it;
}

bool RoomStorage::remove(uint64_t id)
{
    std::shared_ptr<Room> removed;
    {
        std::lock_guard lock(m_mutex);
        auto it = std::find_if(m_rooms.begin(), m_rooms.end(),
                               [id](const auto& room)
                               { return room->getId() == id; });
        if (it == m_rooms.end())
            return false;
        removed = std::move(*it);
        m_rooms.erase(it);
    }
    return true;
}

void RoomStorage::clear()
{
    std::vector<std::shared_ptr<Room>> removed;
    {
        std::lock_guard lock(m_mutex);
        removed.swap(m_rooms);
    }

    removed.clear();
}

} // namespace nexilis::server
