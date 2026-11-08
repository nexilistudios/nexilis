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
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace Nexilis.Client
{
    public class ClientAPI : IDisposable
    {
        private IntPtr _clientApiPtr;
        private bool _disposed;

        /// <summary>
        /// Initializes a new instance of the <see cref="ClientAPI"/> class.
        /// </summary>
        public ClientAPI(ClientConfig config)
        {
            if (config == null)
            {
                throw new ArgumentNullException(nameof(config));
            }

            _clientApiPtr = ClientAPINative.nexilis_client_api_create(config.ConfigPtr);
            if (_clientApiPtr == IntPtr.Zero)
            {
                throw new InvalidOperationException("Failed to create the native client API.");
            }
        }

        /// <summary>
        /// Gets the native pointer to the client API.
        /// </summary>
        public IntPtr ClientApiPtr
        {
            get
            {
                ThrowIfDisposed();
                return _clientApiPtr;
            }
        }

        /// <summary>True once this instance has been disposed.</summary>
        public bool IsDisposed => _disposed;

        /// <summary>
        /// Is the server aware of the client, is the ClientAPI and Packet ready for use.
        /// </summary>
        public bool IsInitialized()
        {
            ThrowIfDisposed();
            return ClientAPINative.nexilis_client_api_is_initialized(_clientApiPtr);
        }

        /// <summary>
        /// Get active rooms from the server.
        /// </summary>
        public RoomsCollection GetActiveRooms()
        {
            ThrowIfDisposed();

            var ptr = ClientAPINative.nexilis_client_api_get_active_rooms(_clientApiPtr);
            if (ptr == IntPtr.Zero)
            {
                throw new InvalidOperationException("Failed to get the active rooms.");
            }
            return new RoomsCollection(ptr);
        }

        /// <summary>
        /// Gets the amount of rooms the client currently knows about. Cheaper
        /// than <see cref="GetActiveRooms"/> when only the count is needed.
        /// </summary>
        public ulong GetActiveRoomsCount()
        {
            ThrowIfDisposed();
            return ClientAPINative.nexilis_client_api_rooms_count(_clientApiPtr);
        }

        /// <summary>
        /// Gets a snapshot of every room the client currently knows about.
        /// The returned values are plain managed data and stay valid after the
        /// underlying native collection has been released.
        /// </summary>
        public IReadOnlyList<RoomInfo> GetActiveRoomInfos()
        {
            using (var rooms = GetActiveRooms())
            {
                return rooms.GetRoomInfos();
            }
        }

        /// <summary>
        /// Get the unique id of "this" client.
        /// </summary>
        public ulong GetClientId()
        {
            ThrowIfDisposed();
            return ClientAPINative.nexilis_client_api_get_client_id(_clientApiPtr);
        }

        /// <summary>
        /// Get the username the server has for a client. Returns an empty
        /// string when the client is not in any known room.
        /// </summary>
        public string GetClientUsername(ulong clientId)
        {
            ThrowIfDisposed();

            var namePtr = ClientAPINative.nexilis_client_api_get_client_username(_clientApiPtr, clientId);
            if (namePtr == IntPtr.Zero)
            {
                return string.Empty;
            }

            try
            {
                return Marshal.PtrToStringAnsi(namePtr) ?? string.Empty;
            }
            finally
            {
                // The native side allocates the string with malloc.
                Marshal.FreeHGlobal(namePtr);
            }
        }

        /// <summary>
        /// Get the id of the room where this client currently is.
        /// </summary>
        public ulong ClientRoomId()
        {
            ThrowIfDisposed();
            return ClientAPINative.nexilis_client_api_client_room_id(_clientApiPtr);
        }

        /// <summary>
        /// True when the client has been placed in a room by the server.
        /// </summary>
        public bool IsInRoom()
        {
            return ClientRoomId() != 0;
        }

        /// <summary>
        /// Get an owned snapshot of a room. Returns null when the room is
        /// unknown to the client. Dispose the returned room when done.
        /// </summary>
        public Room? GetRoomFromId(ulong room_id)
        {
            ThrowIfDisposed();

            var room_ptr = ClientAPINative.nexilis_client_api_get_room(_clientApiPtr, room_id);
            if (room_ptr == IntPtr.Zero)
            {
                return null;
            }
            // Construct room from native pointer.
            return new Room(room_ptr, ownsNativeInstance: true);
        }

        /// <summary>
        /// Get an owned snapshot of a client session. Dispose the returned
        /// session when done.
        /// </summary>
        public ClientSession GetClientFromClientId(ulong clientId)
        {
            ThrowIfDisposed();

            var client_ptr = ClientAPINative.nexilis_client_api_get_client_from_room(_clientApiPtr, clientId);
            if (client_ptr == IntPtr.Zero)
            {
                throw new InvalidOperationException($"Client {clientId} was not found in any room.");
            }
            return new ClientSession(client_ptr, ownsNativeInstance: true);
        }

        /// <summary>Consumes pending shooter events as a UTF-8 JSON object.</summary>
        public string DrainGameEventsJson(int chatSince = 0)
        {
            ThrowIfDisposed();
            var ptr = ClientAPINative.nexilis_client_api_drain_game_events(_clientApiPtr, new UIntPtr((uint)Math.Max(0, chatSince)));
            if (ptr == IntPtr.Zero) return "{}";
            try { return Marshal.PtrToStringAnsi(ptr) ?? "{}"; }
            finally { ClientAPINative.nexilis_client_api_free_events(ptr); }
        }

        void ThrowIfDisposed()
        {
            if (_disposed)
            {
                throw new ObjectDisposedException(nameof(ClientAPI));
            }
        }

        /// <summary>
        /// Releases the resources used by the <see cref="ClientAPI"/>.
        /// </summary>
        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        /// <summary>
        /// Releases the unmanaged resources used by the <see cref="ClientAPI"/> and optionally releases the managed resources.
        /// </summary>
        /// <param name="disposing">True to release both managed and unmanaged resources; false to release only unmanaged resources.</param>
        protected virtual void Dispose(bool disposing)
        {
            if (_disposed)
            {
                return;
            }

            _disposed = true;
            if (_clientApiPtr != IntPtr.Zero)
            {
                ClientAPINative.nexilis_client_api_destroy(_clientApiPtr);
                _clientApiPtr = IntPtr.Zero;
            }
        }

        /// <summary>
        /// Finalizes an instance of the <see cref="ClientAPI"/> class.
        /// </summary>
        ~ClientAPI()
        {
            Dispose(false);
        }
    }
}
