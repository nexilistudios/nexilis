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
    /// <summary>
    /// A message sent to a room by a client session.
    /// </summary>
    public class Communication : IDisposable
    {
        IntPtr _nativePtr;
        bool _disposed = false;
        bool _ownsNativeInstance;

        /// <summary>
        /// Creates a new communication with a payload and a sender.
        /// </summary>
        public Communication(string payload, ClientSession sender)
        {
            if (string.IsNullOrEmpty(payload))
            {
                throw new ArgumentException("Payload must not be null or empty.", nameof(payload));
            }
            if (sender == null)
            {
                throw new ArgumentNullException(nameof(sender));
            }

            _nativePtr = RoomNative.nexilis_communication_create(payload, sender.GetNativePointer());
            if (_nativePtr == IntPtr.Zero)
            {
                throw new InvalidOperationException("Failed to create native communication.");
            }
            _ownsNativeInstance = true;
        }

        internal Communication(IntPtr nativePointer, bool ownsNativeInstance)
        {
            if (nativePointer == IntPtr.Zero)
            {
                throw new ArgumentNullException(nameof(nativePointer));
            }
            _nativePtr = nativePointer;
            _ownsNativeInstance = ownsNativeInstance;
        }

        /// <summary>
        /// Gets the payload of the communication.
        /// </summary>
        public string GetPayload()
        {
            ThrowIfDisposed();

            IntPtr payloadPtr = RoomNative.nexilis_communication_get_payload(_nativePtr);
            if (payloadPtr == IntPtr.Zero)
            {
                throw new InvalidOperationException("Failed to get communication payload.");
            }

            return NativeInterop.TakeString(payloadPtr);
        }

        /// <summary>
        /// Gets the unique id of the communication. Zero until the
        /// communication has been added to a room.
        /// </summary>
        public ulong GetId()
        {
            ThrowIfDisposed();
            return RoomNative.nexilis_communication_get_id(_nativePtr);
        }

        public ulong GetSenderId()
        {
            ThrowIfDisposed();
            return RoomNative.nexilis_communication_get_sender_id(_nativePtr);
        }

        /// <summary>Whether this was stored chat sent when joining a room.</summary>
        public bool WasReplayed()
        {
            ThrowIfDisposed();
            return RoomNative.nexilis_communication_was_replayed(_nativePtr);
        }

        /// <summary>
        /// Gets the native pointer to the sender's session wrapper.
        /// </summary>
        public IntPtr GetSender()
        {
            ThrowIfDisposed();
            return RoomNative.nexilis_communication_get_client(_nativePtr);
        }

        /// <summary>
        /// Gets the native pointer to the communication.
        /// </summary>
        public IntPtr NativePointer => _nativePtr;

        // The room takes ownership of the underlying message (it is moved
        // in natively), so disposal must not destroy it afterwards.
        internal void MarkOwnershipTransferred()
        {
            _ownsNativeInstance = false;
        }

        void ThrowIfDisposed()
        {
            if (_disposed)
            {
                throw new ObjectDisposedException(nameof(Communication));
            }
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!_disposed)
            {
                if (_ownsNativeInstance && _nativePtr != IntPtr.Zero)
                {
                    RoomNative.nexilis_communication_destroy(_nativePtr);
                    _nativePtr = IntPtr.Zero;
                }
                _disposed = true;
            }
        }

        ~Communication()
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
