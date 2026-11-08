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

#ifndef NEXILIS_SERVER_ROOM_LIMITS_HH
#define NEXILIS_SERVER_ROOM_LIMITS_HH

#include <nexilis/nexilis_constants.hh>

#include <cstddef>

namespace nexilis::server
{

/// Limits the server enforces on client-created rooms and room resources. Every limit has a default
/// and is configurable through the server's ServerConfig.
class RoomLimits
{
public:
    /// Maximum number of rooms the server keeps at once.
    void setMaxRooms(std::size_t maxRooms)
    {
        m_maxRooms = maxRooms;
    }
    std::size_t getMaxRooms() const
    {
        return m_maxRooms;
    }

    /// Maximum number of rooms one client may create.
    void setMaxRoomsPerClient(std::size_t maxRoomsPerClient)
    {
        m_maxRoomsPerClient = maxRoomsPerClient;
    }
    std::size_t getMaxRoomsPerClient() const
    {
        return m_maxRoomsPerClient;
    }

    /// Maximum ObjectsPerRoom; zero rejects client requests for this resource.
    void setMaxObjectsPerRoom(std::size_t value)
    {
        m_maxObjectsPerRoom = value;
    }
    std::size_t getMaxObjectsPerRoom() const
    {
        return m_maxObjectsPerRoom;
    }

    /// Maximum GameItemsPerRoom; zero rejects client requests for this resource.
    void setMaxGameItemsPerRoom(std::size_t value)
    {
        m_maxGameItemsPerRoom = value;
    }
    std::size_t getMaxGameItemsPerRoom() const
    {
        return m_maxGameItemsPerRoom;
    }

    /// Maximum UsernameLength; zero rejects client requests for this resource.
    void setMaxUsernameLength(std::size_t value)
    {
        m_maxUsernameLength = value;
    }
    std::size_t getMaxUsernameLength() const
    {
        return m_maxUsernameLength;
    }

    /// Whether the limits the static command handlers enforce have been
    /// bound to a live configuration.
    static bool isInitialized()
    {
        return m_limits != nullptr;
    }

    /// Bind the limits the static command handlers enforce. The pointer is
    /// refreshed on every dispatch so a fresh Command does not leave handlers
    /// reading a destroyed configuration's limits.
    static void _initialize(const RoomLimits& limits)
    {
        m_limits = &limits;
    }

    /// The limits the static command handlers enforce, or the defaults when
    /// no configuration has been bound yet.
    static const RoomLimits& get()
    {
        static const RoomLimits defaults;
        return m_limits ? *m_limits : defaults;
    }

private:
    std::size_t m_maxRooms = NEXILIS_DEFAULT_MAX_ROOMS;
    std::size_t m_maxRoomsPerClient = NEXILIS_DEFAULT_MAX_ROOMS_PER_CLIENT;

    std::size_t m_maxObjectsPerRoom = 1024;
    std::size_t m_maxGameItemsPerRoom = 1024;
    std::size_t m_maxUsernameLength = 64;

    static inline const RoomLimits* m_limits = nullptr;
};

} // namespace nexilis::server

#endif
