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
    public class Room : IDisposable
    {
        IntPtr _nativePtr;
        bool _disposed = false;
        bool _ownsNativeInstance;

        public Room(RoomData roomData)
        {
            if (roomData.NativePointer == IntPtr.Zero)
            {
                throw new ArgumentNullException(nameof(roomData));
            }
            _nativePtr = RoomNative.nexilis_room_create(roomData.NativePointer, IntPtr.Zero);
            _ownsNativeInstance = true;
        }

        /// <summary>
        /// Wraps a room that already exists natively. The wrapper only frees
        /// the native handle when <paramref name="ownsNativeInstance"/> is set;
        /// pass false for rooms that are owned by somebody else, e.g. a
        /// <see cref="RoomsCollection"/> whose pointers are reused between
        /// calls.
        /// </summary>
        public Room(IntPtr nativePointer, bool ownsNativeInstance)
        {
            if (nativePointer == IntPtr.Zero)
            {
                throw new ArgumentNullException(nameof(nativePointer));
            }
            _nativePtr = nativePointer;
            _ownsNativeInstance = ownsNativeInstance;
        }

        public IReadOnlyList<ClientSession> GetClients()
        {
            ThrowIfDisposed();

            ulong numClients;
            IntPtr clientsArrayPtr = RoomNative.nexilis_room_get_clients(_nativePtr, out numClients);

            if (clientsArrayPtr == IntPtr.Zero || numClients == 0)
            {
                return Array.Empty<ClientSession>();
            }

            var clientSessions = new List<ClientSession>();
            IntPtr[] clientPointers = new IntPtr[numClients];

            try
            {
                Marshal.Copy(clientsArrayPtr, clientPointers, 0, (int)numClients);

                // Create managed wrappers. Each wrapper borrows its native
                // ClientSession handle from the room's client array; the
                // underlying ClientSession is owned by the ClientAPI and
                // must not be destroyed here.
                foreach (var ptr in clientPointers)
                {
                    if (ptr != IntPtr.Zero)
                    {
                        clientSessions.Add(new ClientSession(ptr, ownsNativeInstance: false));
                    }
                }
                return clientSessions.AsReadOnly();
            }
            finally
            {
                // Free the outer array and the per-element nexilis_ClientSession
                // wrapper structs (which only hold a pointer to the live
                // ClientSession owned by the ClientAPI). The underlying
                // ClientSession objects are untouched.
                RoomNative.nexilis_room_free_client_array(clientsArrayPtr, numClients);
            }

        }

        public ulong GetClientAmount()
        {
            ThrowIfDisposed();
            return RoomNative.nexilis_room_get_client_amount(_nativePtr);
        }

        public ulong GetId()
        {
            ThrowIfDisposed();
            return RoomNative.nexilis_room_get_id(_nativePtr);
        }

        /// <summary>
        /// Gets the name of the room.
        /// </summary>
        public string GetName()
        {
            ThrowIfDisposed();

            var namePtr = RoomNative.nexilis_room_get_name(_nativePtr);
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
        /// Gets the 2D/3D context of the room.
        /// </summary>
        public RoomContext GetContext()
        {
            ThrowIfDisposed();
            return RoomNative.nexilis_room_get_context(_nativePtr);
        }

        /// <summary>
        /// Gets the maximum amount of clients the room accepts.
        /// </summary>
        public uint GetMaxSize()
        {
            ThrowIfDisposed();
            return RoomNative.nexilis_room_get_max_size(_nativePtr);
        }

        /// <summary>
        /// Gets the id of the client that created the room.
        /// </summary>
        public ulong GetCreatorId()
        {
            ThrowIfDisposed();
            return RoomNative.nexilis_room_get_creator_id(_nativePtr);
        }

        /// <summary>
        /// Gets the native pointer to the room.
        /// </summary>
        public IntPtr GetNativePointer()
        {
            ThrowIfDisposed();
            return _nativePtr;
        }

        public void AddClient(ClientSession client)
        {
            ThrowIfDisposed();
            if (client == null)
                throw new ArgumentNullException(nameof(client));

            RoomNative.nexilis_room_add_client(_nativePtr, client.GetNativePointer());
        }

        public void RemoveClient(ulong clientId)
        {
            ThrowIfDisposed();
            RoomNative.nexilis_room_remove_client(_nativePtr, clientId);
        }

        /// <summary>
        /// Adds a message to the room. The room takes ownership of the
        /// communication; it must not be disposed by the caller afterwards.
        /// </summary>
        public void AddMessage(Communication message)
        {
            ThrowIfDisposed();
            if (message == null)
            {
                throw new ArgumentNullException(nameof(message));
            }

            RoomNative.nexilis_room_add_message(_nativePtr, message.NativePointer);
            message.MarkOwnershipTransferred();
        }

        /// <summary>
        /// Gets copies of all messages in the room.
        /// </summary>
        public IReadOnlyList<Communication> GetMessages()
        {
            ThrowIfDisposed();

            ulong numMessages;
            IntPtr messagesArrayPtr = RoomNative.nexilis_room_get_messages(_nativePtr, out numMessages);

            if (messagesArrayPtr == IntPtr.Zero || numMessages == 0)
            {
                return Array.Empty<Communication>();
            }

            var communications = new List<Communication>();
            IntPtr[] messagePointers = new IntPtr[numMessages];

            try
            {
                Marshal.Copy(messagesArrayPtr, messagePointers, 0, (int)numMessages);

                foreach (var ptr in messagePointers)
                {
                    if (ptr != IntPtr.Zero)
                    {
                        communications.Add(new Communication(ptr, ownsNativeInstance: true));
                    }
                }
                return communications.AsReadOnly();
            }
            finally
            {
                // Free only the outer array; each Communication wrapper owns
                // its copied native instance and releases it on Dispose.
                RoomNative.nexilis_room_free_message_array(messagesArrayPtr, 0);
            }
        }

        /// <summary>
        /// Checks whether the room contains the given communication.
        /// </summary>
        public bool ContainsCommunication(Communication message)
        {
            ThrowIfDisposed();
            if (message == null)
            {
                throw new ArgumentNullException(nameof(message));
            }
            return RoomNative.nexilis_room_contains_communication(_nativePtr, message.NativePointer);
        }

        /// <summary>
        /// Checks whether the room contains a communication with the given id.
        /// </summary>
        public bool ContainsCommunication(ulong messageId)
        {
            ThrowIfDisposed();
            return RoomNative.nexilis_room_contains_communication_by_id(_nativePtr, messageId);
        }

        void ThrowIfDisposed()
        {
            if (_disposed)
            {
                throw new ObjectDisposedException(nameof(Room));
            }
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!_disposed)
            {
                if (_ownsNativeInstance && _nativePtr != IntPtr.Zero)
                {
                    RoomNative.nexilis_room_destroy(_nativePtr);
                }
                _nativePtr = IntPtr.Zero;
                _disposed = true;
            }
        }

        ~Room()
        {
            Dispose(false);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }
    }
}
