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

namespace Nexilis.Client
{
    public static class ClientAPINative
    {
        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr nexilis_client_api_create(IntPtr serverData);

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nexilis_client_api_destroy(IntPtr client);

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool nexilis_client_api_is_boost_tcp_ready(IntPtr client_api);
        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool nexilis_client_api_is_boost_udp_ready(IntPtr client_api);

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.U1)]
        public static extern bool nexilis_client_api_is_initialized(IntPtr client_api);

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nexilis_client_api_wait_until_boost_tcp_ready(IntPtr client_api);
        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nexilis_client_api_wait_until_boost_udp_ready(IntPtr client_api);

        [DllImport(NativeLibrary.Name,
            EntryPoint = "nexilis_client_api_get_boost_tcp_server_address",
            CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr get_boost_tcp_server_address_raw(IntPtr client_api);
        public static string nexilis_client_api_get_boost_tcp_server_address(IntPtr client_api) =>
            NativeInterop.TakeString(get_boost_tcp_server_address_raw(client_api));
        [DllImport(NativeLibrary.Name,
            EntryPoint = "nexilis_client_api_get_boost_udp_server_address",
            CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr get_boost_udp_server_address_raw(IntPtr client_api);
        public static string nexilis_client_api_get_boost_udp_server_address(IntPtr client_api) =>
            NativeInterop.TakeString(get_boost_udp_server_address_raw(client_api));

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern ulong nexilis_client_api_get_client_id(IntPtr client_api);

        // Returns a malloc'd string that the caller has to free, so the
        // pointer is returned instead of a managed string.
        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr nexilis_client_api_get_client_username(IntPtr client_api, ulong client_id);
        [DllImport(NativeLibrary.Name, EntryPoint = "nexilis_client_api_get_client_password", CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr get_client_password_raw(IntPtr client_api);
        public static string nexilis_client_api_get_client_password(IntPtr client_api) =>
            NativeInterop.TakeString(get_client_password_raw(client_api));

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr nexilis_client_api_get_active_rooms(IntPtr client_api);
        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern ulong nexilis_client_api_rooms_count(IntPtr client_api);

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern ulong nexilis_client_api_client_room_id(IntPtr client_api);

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr nexilis_client_api_get_room(IntPtr client_api, ulong room_id);

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr nexilis_client_api_get_client_from_room(IntPtr client_api, ulong client_id);

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr nexilis_client_api_drain_game_events(IntPtr client_api, UIntPtr chatSince);
        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nexilis_client_api_free_events(IntPtr events);
    }
}
