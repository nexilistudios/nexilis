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

#ifndef NEXILIS_ROOM_DATA_HH
#define NEXILIS_ROOM_DATA_HH

#include <nexilis/util.hh>

namespace nexilis
{

class RoomData
{
public:
    enum class Context
    {
        _2D,
        _3D
    };

    /// Constructor.
    /// \param creatorId The identifier of the creator.
    /// \param name Room name. Client creation requests accept 1-64 bytes.
    /// \param roomId The id of the created room.
    /// \param context The context for dimensions in a room.
    /// \param maxSize The maximum size of the room.
    RoomData(uint64_t creatorId, const std::string& name, uint64_t roomId, Context context = Context::_3D, uint32_t maxSize = NEXILIS_DEFAULT_ROOM_CLIENT_AMOUNT);

    /// Copy constructor.
    RoomData(const RoomData& other);

    /// Move constructor.
    RoomData(RoomData&& other);

    /// Copy assignment operator.
    RoomData& operator=(const RoomData& other);

    /// Move assignment operator.
    RoomData& operator=(RoomData&& other);

    /// Get the given name for the room.
    const std::string& getName() const
    {
        return m_name;
    }

    /// Get the context of the room.
    Context getContext() const
    {
        return m_context;
    }

    /// Get the maximum amount of players in a room.
    uint32_t getMaxSize() const
    {
        return m_maxSize;
    }

    /// Get the identifier of the room.
    uint64_t getId() const
    {
        return m_roomId;
    }

    uint64_t getCreatorId() const
    {
        return m_creatorId;
    }

private:
    /// Id of the creator of this room.
    uint64_t m_creatorId;

    /// The name of this room.
    std::string m_name;

    /// The unique identifier of this room.
    uint64_t m_roomId;

    // The context of the room.
    Context m_context;

    /// The maximum amount of players in a room.
    uint32_t m_maxSize;
};

} // namespace nexilis

#endif
