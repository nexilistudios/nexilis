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

#ifndef NEXILIS_ROOM_COMMAND_TYPE_HH
#define NEXILIS_ROOM_COMMAND_TYPE_HH

#include <nexilis/types/vector2.hh>
#include <nexilis/types/vector3.hh>

#include <cstdint>
#include <string>

namespace nexilis
{

class RoomCommandType
{
public:
    enum class Root : uint8_t
    {
        management = 0,
        communication = 1,
        player_2D = 2,
        object_2D = 3,
        player_3D = 4,
        object_3D = 5,
        game_item = 6,
        undefined = 7
    };

    enum class Management : uint8_t
    {
        join = 0,
        leave = 1,
        create = 2,
        remove = 3,
        set_overlap = 4
    };

    enum class Communication : uint8_t
    {
        broadcast,
        othercast,
        unicast,
        /// Like broadcast, but the payload is a base64 ciphertext blob.
        broadcast_encrypted
    };

    enum class PlayerType : uint8_t
    {
        position = 0,
        dimension = 1,
        movement = 2,
        shoot = 3,
        respawn = 4,
        /// Set the team a player is playing on (client -> server).
        set_team = 5,
        /// Either a request for the current kill/death leaderboard
        /// (client -> server) or the leaderboard payload itself (server -> client).
        leaderboard = 6,
        /// A positional sound event (e.g. a footstep or a gunshot) so other
        /// clients can spatialize where the sound happened.
        audio_event = 7,
        match_action = 8
    };

    enum class ObjectType : uint8_t
    {
        create = 0,
        destroy = 1,
        move = 2,
        create_moving = 3
    };

    enum class GameItemAction : uint8_t
    {
        create = 0,
        update = 1,
        destroy = 2
    };

    static std::string RoomTypeToString(Root type);

    static std::string ManagementTypeToString(Management management);
    static std::string CommunicationTypeToString(Communication communication);
    static std::string PlayerTypeToString(PlayerType player);
    static std::string ObjectTypeToString(ObjectType object);
    static std::string GameItemActionToString(GameItemAction action);

    template <typename VectorType>
    static Root getRootPlayerType(const VectorType& v)
    {
        if constexpr (std::is_same_v<VectorType, Vector2<decltype(v)>>)
        {
            return Root::player_2D;
        }
        else if constexpr (std::is_same_v<VectorType, Vector3<decltype(v)>>)
        {
            return Root::player_3D;
        }
        return Root::undefined;
    }

    template <typename VectorType>
    static Root getRootObjectType(const VectorType& v)
    {
        if constexpr (std::is_same_v<VectorType, Vector2<decltype(v)>>)
        {
            return Root::object_2D;
        }
        else if constexpr (std::is_same_v<VectorType, Vector3<decltype(v)>>)
        {
            return Root::object_3D;
        }
        return Root::undefined;
    }
};

} // namespace nexilis

#endif
