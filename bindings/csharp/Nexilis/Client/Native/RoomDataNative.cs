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

using System.Runtime.InteropServices;

namespace Nexilis.Client
{
    public class RoomDataNative
    {
        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr nexilis_room_data_create(
            ulong creatorId,
            [MarshalAs(UnmanagedType.LPStr)] string name,
            RoomContext context,
            uint maxSize);

        [DllImport(NativeLibrary.Name, EntryPoint = "nexilis_room_data_create", CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr create_utf8(ulong creatorId, byte[] name, RoomContext context, uint maxSize);

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nexilis_room_data_destroy(IntPtr roomData);

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr nexilis_room_data_get_name(IntPtr roomData);

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern RoomContext nexilis_room_data_get_context(IntPtr roomData);

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern uint nexilis_room_data_get_max_size(IntPtr roomData);

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern ulong nexilis_room_data_get_id(IntPtr roomData);

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern ulong nexilis_room_data_get_creator_id(IntPtr roomData);
    }
}
