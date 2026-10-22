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

using System;
using System.Runtime.InteropServices;

namespace Nexilis
{
    public static class Packet
    {
        static NxLogger _logger = new NxLogger("Packet");
        public static void InitializeLogger(Action<Logger.LogLevel, string> logCallback) => _logger.Setup(logCallback);

        static NxData Wrap(RawNxData raw, string operation)
        {
            if (raw.data == IntPtr.Zero) throw new InvalidOperationException(operation + " failed.");
            return NxData.Create(raw);
        }

        public static NxData Player3DSetTeam(IntPtr clientApi, string team) =>
            Wrap(PacketNative.nexilis_packet_room_player3D_set_team(clientApi, team), "Set team");

        [Obsolete("Servers reject client-selected target and damage. Use Player3DFire instead.")]
        public static NxData Player3DShoot(IntPtr clientApi, ulong targetId, float damage) =>
            Wrap(PacketNative.nexilis_packet_room_player3D_shoot(clientApi, targetId, damage), "Shoot");

        public static NxData Player3DFire(IntPtr clientApi, float x, float y, float z, byte slot) =>
            Wrap(PacketNative.nexilis_packet_room_player3D_fire(clientApi, x, y, z, slot), "Fire");

        public static NxData Player3DAudioEvent(IntPtr clientApi, byte sound, float x, float y, float z) =>
            Wrap(PacketNative.nexilis_packet_room_player3D_audio_event(clientApi, sound, x, y, z), "Audio event");

        public static NxData RoomBroadcast(IntPtr clientApi, string message) =>
            Wrap(PacketNative.nexilis_packet_room_communicate_broadcast(clientApi, message), "Broadcast");

        public static NxData GetGeneralClientId(IntPtr clientApi)
        {
            return NativeInterop.ExecuteSafe(() =>
            {
                var raw = PacketNative.nexilis_packet_get_general_clientId(clientApi);
                if (raw.data == IntPtr.Zero)
                {
                    throw new InvalidOperationException("Failed to get general clientId.");
                }
                return NxData.Create(raw);
            }, "GetGeneralClientId");
        }

        public static NxData InfoGeneral(IntPtr clientApi)
        {
            return NativeInterop.ExecuteSafe(() =>
            {
                var raw = PacketNative.nexilis_packet_get_info_general(clientApi);
                if (raw.data == IntPtr.Zero)
                {
                    throw new InvalidOperationException("Failed to get packet info general.");
                }
                return NxData.Create(raw);
            }, "InfoGeneral");
        }

        public static NxData InfoClients(IntPtr clientApi)
        {
            return NativeInterop.ExecuteSafe(() =>
            {
                var raw = PacketNative.nexilis_packet_get_info_clients(clientApi);
                if (raw.data == IntPtr.Zero)
                {
                    throw new InvalidOperationException("Failed to get packet info clients.");
                }
                return NxData.Create(raw);
            }, "InfoClients");
        }

        public static NxData InfoRooms(IntPtr clientApi)
        {
            return NativeInterop.ExecuteSafe(() =>
            {
                var raw = PacketNative.nexilis_packet_get_info_rooms(clientApi);
                if (raw.data == IntPtr.Zero)
                {
                    throw new InvalidOperationException("Failed to get packet info rooms.");
                }
                return NxData.Create(raw);
            }, "InfoRooms");
        }

        public static NxData RoomManagementCreate(IntPtr clientApi, RoomContext context, string roomName)
        {
            return NativeInterop.ExecuteSafe(() =>
            {
                var raw = PacketNative.nexilis_packet_room_management_create(clientApi, roomName, context);
                if (raw.data == IntPtr.Zero)
                {
                    throw new InvalidOperationException("Failed to get room management create.");
                }
                return NxData.Create(raw);
            }, "RoomManagementCreate");
        }
        public static NxData RoomManagementJoin(IntPtr clientApi, ulong roomId)
        {
            return NativeInterop.ExecuteSafe(() =>
            {
                var raw = PacketNative.nexilis_packet_room_management_join(clientApi, roomId);
                if (raw.data == IntPtr.Zero)
                {
                    throw new InvalidOperationException("Failed to get room management join.");
                }
                return NxData.Create(raw);
            }, "RoomManagementJoin");
        }

        public static NxData RoomManagementLeave(IntPtr clientApi)
        {
            return NativeInterop.ExecuteSafe(() =>
            {
                var raw = PacketNative.nexilis_packet_room_management_leave(clientApi);
                if (raw.data == IntPtr.Zero)
                {
                    throw new InvalidOperationException("Failed to get room management leave.");
                }
                return NxData.Create(raw);
            }, "RoomManagementLeave");
        }

        public static NxData Player3DPosition(IntPtr clientApi, Vector3<float> position)
        {
            return NativeInterop.ExecuteSafe(() =>
            {
                var raw = PacketNative.nexilis_packet_room_player3D_position(clientApi, position.getNative().vec);
                if (raw.data == IntPtr.Zero)
                {
                    throw new InvalidOperationException("Failed to get room player3D position.");
                }
                return NxData.Create(raw);
            }, "Player3DPosition");
        }
        public static NxData Player3DPositionDirect(IntPtr clientApi, float x, float y, float z)
        {
            return NativeInterop.ExecuteSafe(() =>
            {
                var raw = PacketNative.nexilis_packet_room_player3D_position_direct(clientApi, x, y, z);
                if (raw.data == IntPtr.Zero)
                {
                    throw new InvalidOperationException("Native call returned null");
                }
                return NxData.Create(raw);
            }, "Player3DPositionDirect");
        }
        public static NxData Player3DDimension(IntPtr clientApi, Vector3<float> dimensions)
        {
            return NativeInterop.ExecuteSafe(() =>
            {
                var raw = PacketNative.nexilis_packet_room_player3D_dimension(clientApi, dimensions.getNative().vec);
                if (raw.data == IntPtr.Zero)
                {
                    throw new InvalidOperationException("Failed to get room player3D dimension.");
                }
                return NxData.Create(raw);
            }, "Player3DDimension");
        }

        public static NxData Player3DMovement(IntPtr clientApi, Vector3<float> movement, float deltatime)
        {
            return NativeInterop.ExecuteSafe(() =>
            {
                var raw = PacketNative.nexilis_packet_room_player3D_movement(clientApi, movement.getNative().vec, deltatime);
                if (raw.data == IntPtr.Zero)
                {
                    throw new InvalidOperationException("Failed to get room player3D movement.");
                }
                return NxData.Create(raw);
            }, "Player3DMovement");
        }

        public static NxData Player3DMovementDirect(IntPtr clientApi, float x, float y, float z, float deltatime)
        {
            return NativeInterop.ExecuteSafe(() =>
            {
                var raw = PacketNative.nexilis_packet_room_player3D_movement_direct(clientApi, x, y, z, deltatime);
                if (raw.data == IntPtr.Zero)
                {
                    throw new InvalidOperationException("Failed to call nexilis_packet_room_player3D_movement_direct");
                }
                return NxData.Create(raw);
            }, "Player3DMovementDirect");
        }
    }
}
