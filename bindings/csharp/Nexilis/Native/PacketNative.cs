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
    public static class PacketNative
    {
        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern RawNxData nexilis_packet_get_general_clientId(IntPtr clientApi);

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern RawNxData nexilis_packet_get_info_general(IntPtr clientApi);
        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern RawNxData nexilis_packet_get_info_clients(IntPtr clientApi);
        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern RawNxData nexilis_packet_get_info_rooms(IntPtr clientApi);


        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern RawNxData nexilis_packet_room_management_join(IntPtr clientApi, ulong roomId);
        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern RawNxData nexilis_packet_room_management_leave(IntPtr clientApi);
        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern RawNxData nexilis_packet_room_management_create(IntPtr clientApi, string roomName, RoomContext context);


        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern RawNxData nexilis_packet_room_player3D_position(IntPtr clientApi, IntPtr vec);
        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern RawNxData nexilis_packet_room_player3D_position_direct(IntPtr clientApi, float x, float y, float z);
        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern RawNxData nexilis_packet_room_player3D_dimension(IntPtr clientApi, IntPtr vector);
        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern RawNxData nexilis_packet_room_player3D_movement(IntPtr clientApi, IntPtr vector, float deltaTime);
        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern RawNxData nexilis_packet_room_player3D_movement_direct(IntPtr clientApi,
                float x, float y, float z, float deltatime);
        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern RawNxData nexilis_packet_room_player3D_set_team(IntPtr clientApi, string team);
        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern RawNxData nexilis_packet_room_player3D_shoot(IntPtr clientApi, ulong targetId, float damage);

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern RawNxData nexilis_packet_room_player3D_fire(IntPtr clientApi, float x, float y, float z, byte slot);
        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern RawNxData nexilis_packet_room_player3D_audio_event(IntPtr clientApi, byte sound, float x, float y, float z);
        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern RawNxData nexilis_packet_room_communicate_broadcast(IntPtr clientApi, string message);
    }

}
