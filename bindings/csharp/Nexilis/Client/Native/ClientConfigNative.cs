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
    public static class ClientConfigNative
    {
        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr nexilis_client_config_create();

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nexilis_client_config_destroy(IntPtr config);


        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nexilis_client_config_set_password(IntPtr config, string password);
        [DllImport(NativeLibrary.Name, EntryPoint = "nexilis_client_config_set_password", CallingConvention = CallingConvention.Cdecl)]
        internal static extern void set_password_utf8(IntPtr config, byte[] password);

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nexilis_client_config_set_account_credentials(IntPtr config, string accountId, string credential);
        [DllImport(NativeLibrary.Name,
            EntryPoint = "nexilis_client_config_set_account_credentials",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern void set_account_credentials_utf8(IntPtr config, byte[] accountId, byte[] credential);

        [DllImport(NativeLibrary.Name, EntryPoint = "nexilis_client_config_get_password", CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr get_password_raw(IntPtr config);

        public static string nexilis_client_config_get_password(IntPtr config) => NativeInterop.TakeString(get_password_raw(config));

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nexilis_string_free(IntPtr value);

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nexilis_client_config_set_mode(IntPtr config, int mode);

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nexilis_client_config_set_message_encryption(IntPtr config, int enabled);

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nexilis_client_config_get_message_encryption(IntPtr config);

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nexilis_client_config_set_tls(IntPtr config, int enabled);

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern int nexilis_client_config_get_tls(IntPtr config);

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nexilis_client_config_set_server_public_key_sha256(IntPtr config, string hex);

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nexilis_client_config_set_boost_tcp_address(IntPtr config, string address);
        [DllImport(NativeLibrary.Name,
            EntryPoint = "nexilis_client_config_set_boost_tcp_address",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern void set_boost_tcp_address_utf8(IntPtr config, byte[] address);

        [DllImport(NativeLibrary.Name,
            EntryPoint = "nexilis_client_config_get_boost_tcp_server_address",
            CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr get_boost_tcp_server_address_raw(IntPtr config);

        public static string nexilis_client_config_get_boost_tcp_server_address(IntPtr config) =>
            NativeInterop.TakeString(get_boost_tcp_server_address_raw(config));

        [DllImport(NativeLibrary.Name, CallingConvention = CallingConvention.Cdecl)]
        public static extern void nexilis_client_config_set_boost_udp_address(IntPtr config, string address);
        [DllImport(NativeLibrary.Name,
            EntryPoint = "nexilis_client_config_set_boost_udp_address",
            CallingConvention = CallingConvention.Cdecl)]
        internal static extern void set_boost_udp_address_utf8(IntPtr config, byte[] address);

        [DllImport(NativeLibrary.Name,
            EntryPoint = "nexilis_client_config_get_boost_udp_server_address",
            CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr get_boost_udp_server_address_raw(IntPtr config);

        public static string nexilis_client_config_get_boost_udp_server_address(IntPtr config) =>
            NativeInterop.TakeString(get_boost_udp_server_address_raw(config));
    }
}
