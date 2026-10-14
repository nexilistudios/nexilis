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

#ifndef NEXILIS_COMMAND_PACKET_BASE_HH
#define NEXILIS_COMMAND_PACKET_BASE_HH

#include <utility>

/// Shared static dispatch surface for client and server operations.
///
/// This template is the Curiously Recurring Template Pattern (CRTP)-style base:
/// it is parameterized by its implementation type \p Impl and forwards every
/// call to a matching static function on \p Impl. The nested `Set`, `Get` and
/// `Room` structs group operations like namespaces, and each leaf forwards its
/// arguments with std::forward, so dispatch is resolved at compile time with no
/// virtual functions.
///
/// The server instantiates it as `Commands = CommandPacketBase<ServerImpl>`
/// (see server/command/commands.hh) and the client as
/// `Packet = CommandPacketBase<ClientImpl>` (see client/packet.hh).
/// See docs/commands.md for the full description.
template <typename Impl>
class CommandPacketBase
{
public:
    struct Set
    {
        struct General
        {
            template <typename... Args>
            static auto clientId(Args&&... args)
            {
                return Impl::set_general_clientId(std::forward<Args>(args)...);
            }
            template <typename... Args>
            static auto username(Args&&... args)
            {
                return Impl::set_general_username(std::forward<Args>(args)...);
            }
        };
        struct Protocol
        {
            struct BoostTCP
            {
                template <typename... Args>
                static auto port(Args&&... args)
                {
                    return Impl::set_protocol_boosttcp_port(std::forward<Args>(args)...);
                }
            };
        };
    };

    struct Get
    {
        struct General
        {
            template <typename... Args>
            static auto clientId(Args&&... args)
            {
                return Impl::get_general_clientId(std::forward<Args>(args)...);
            }
            template <typename... Args>
            static auto roomId(Args&&... args)
            {
                return Impl::get_general_roomId(std::forward<Args>(args)...);
            }
        };
        struct Info
        {
            template <typename... Args>
            static auto general(Args&&... args)
            {
                return Impl::get_info_general(std::forward<Args>(args)...);
            }
            template <typename... Args>
            static auto clients(Args&&... args)
            {
                return Impl::get_info_clients(std::forward<Args>(args)...);
            }
            template <typename... Args>
            static auto rooms(Args&&... args)
            {
                return Impl::get_info_rooms(std::forward<Args>(args)...);
            }
        };
    };

    struct Room
    {
        struct Management
        {
            template <typename... Args>
            static auto join(Args&&... args)
            {
                return Impl::room_management_join(std::forward<Args>(args)...);
            }
            template <typename... Args>
            static auto leave(Args&&... args)
            {
                return Impl::room_management_leave(std::forward<Args>(args)...);
            }
            template <typename... Args>
            static auto create(Args&&... args)
            {
                return Impl::room_management_create(std::forward<Args>(args)...);
            }
            template <typename... Args>
            static auto remove(Args&&... args)
            {
                return Impl::room_management_remove(std::forward<Args>(args)...);
            }
            template <typename... Args>
            static auto setOverlap(Args&&... args)
            {
                return Impl::room_management_setOverlap(std::forward<Args>(args)...);
            }
        };
        struct Communicate
        {
            template <typename... Args>
            static auto broadcast(Args&&... args)
            {
                return Impl::room_communicate_broadcast(std::forward<Args>(args)...);
            }

            template <typename... Args>
            static auto othercast(Args&&... args)
            {
                return Impl::room_communicate_othercast(std::forward<Args>(args)...);
            }

            template <typename... Args>
            static auto unicast(Args&&... args)
            {
                return Impl::room_communicate_unicast(std::forward<Args>(args)...);
            }
        };

        struct Player2D
        {
            template <typename... Args>
            static auto position(Args&&... args)
            {
                return Impl::room_player2d_position(std::forward<Args>(args)...);
            }

            template <typename... Args>
            static auto dimension(Args&&... args)
            {
                return Impl::room_player2d_dimension(std::forward<Args>(args)...);
            }

            template <typename... Args>
            static auto movement(Args&&... args)
            {
                return Impl::room_player2d_movement(std::forward<Args>(args)...);
            }

            template <typename... Args>
            static auto shoot(Args&&... args)
            {
                return Impl::room_player2d_shoot(std::forward<Args>(args)...);
            }
        };

        struct Object2D
        {
            template <typename... Args>
            static auto create(Args&&... args)
            {
                return Impl::room_object2d_create(std::forward<Args>(args)...);
            }
            template <typename... Args>
            static auto destroy(Args&&... args)
            {
                return Impl::room_object2d_destroy(std::forward<Args>(args)...);
            }
            template <typename... Args>
            static auto move(Args&&... args)
            {
                return Impl::room_object2d_move(std::forward<Args>(args)...);
            }
            template <typename... Args>
            static auto createMoving(Args&&... args)
            {
                return Impl::room_object2d_createMoving(std::forward<Args>(args)...);
            }
        };

        struct Player3D
        {
            template <typename... Args>
            static auto position(Args&&... args)
            {
                return Impl::room_player3d_position(std::forward<Args>(args)...);
            }
            template <typename... Args>
            static auto dimension(Args&&... args)
            {
                return Impl::room_player3d_dimension(std::forward<Args>(args)...);
            }
            template <typename... Args>
            static auto movement(Args&&... args)
            {
                return Impl::room_player3d_movement(std::forward<Args>(args)...);
            }
            template <typename... Args>
            static auto shoot(Args&&... args)
            {
                return Impl::room_player3d_shoot(std::forward<Args>(args)...);
            }
            template <typename... Args>
            static auto setTeam(Args&&... args)
            {
                return Impl::room_player3d_set_team(std::forward<Args>(args)...);
            }
            template <typename... Args>
            static auto audioEvent(Args&&... args)
            {
                return Impl::room_player3d_audio_event(std::forward<Args>(args)...);
            }
            template <typename... Args>
            static auto matchAction(Args&&... args)
            {
                return Impl::room_player3d_match_action(std::forward<Args>(args)...);
            }
        };

        struct Object3D
        {
            template <typename... Args>
            static auto create(Args&&... args)
            {
                return Impl::room_object3d_create(std::forward<Args>(args)...);
            }
            template <typename... Args>
            static auto destroy(Args&&... args)
            {
                return Impl::room_object3d_destroy(std::forward<Args>(args)...);
            }
            template <typename... Args>
            static auto move(Args&&... args)
            {
                return Impl::room_object3d_move(std::forward<Args>(args)...);
            }
            template <typename... Args>
            static auto createMoving(Args&&... args)
            {
                return Impl::room_object3d_createmoving(std::forward<Args>(args)...);
            }
        };

        struct GameItem
        {
            template <typename... Args>
            static auto create(Args&&... args)
            {
                return Impl::room_gameitem_create(std::forward<Args>(args)...);
            }
            template <typename... Args>
            static auto update(Args&&... args)
            {
                return Impl::room_gameitem_update(std::forward<Args>(args)...);
            }
            template <typename... Args>
            static auto destroy(Args&&... args)
            {
                return Impl::room_gameitem_destroy(std::forward<Args>(args)...);
            }
        };
    };
};

#endif
