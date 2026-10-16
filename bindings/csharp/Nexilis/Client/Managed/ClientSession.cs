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

namespace Nexilis.Client
{
    public class ClientSession : IDisposable
    {
        IntPtr _nativePointer;
        bool _disposed = false;
        ulong _id;
        readonly bool _ownsNativeInstance;

        public ClientSession(ulong id, IntPtr clientApiHandle, bool ownsNativeInstance)
        {
            if (clientApiHandle == IntPtr.Zero)
            {
                throw new ArgumentNullException(nameof(clientApiHandle));
            }
            _id = id;
            _nativePointer = ClientSessionNative.nexilis_client_session_create(_id, clientApiHandle);
            _ownsNativeInstance = ownsNativeInstance;
        }

        public ClientSession(IntPtr nativePointer, bool ownsNativeInstance)
        {
            if (nativePointer == IntPtr.Zero)
            {
                throw new ArgumentNullException(nameof(nativePointer));
            }
            _nativePointer = nativePointer;
            _id = ClientSessionNative.nexilis_client_session_get_id(_nativePointer);
            _ownsNativeInstance = ownsNativeInstance;
        }

        public void SetPosition3D(float x, float y, float z)
        {
            ThrowIfDisposed();
            ClientSessionNative.nexilis_client_session_set_position_3D(_nativePointer, x, y, z);
        }

        public Vector3<float> GetPosition3D()
        {
            ThrowIfDisposed();
            if (_nativePointer == IntPtr.Zero)
            {
                throw new InvalidOperationException("Native pointer is null");
            }
            float x, y, z;
            var success = ClientSessionNative.nexilis_client_session_get_position_3D_values(_nativePointer, out x, out y, out z);
            if (!success)
            {
                throw new InvalidOperationException("Failed to get position values");
            }
            string l = "Got valid position x:" + x + " y:" + y + " z:" + z;
            FileLog.Log(l);
            return new Vector3<float>(x, y, z);
        }

        public ulong GetId()
        {
            ThrowIfDisposed();
            return _id;
        }

        public IntPtr GetNativePointer()
        {
            ThrowIfDisposed();
            return _nativePointer;
        }

        public void SetUsername(string username)
        {
            ThrowIfDisposed();
            ClientSessionNative.nexilis_client_session_set_username(_nativePointer, username);
        }

        void ThrowIfDisposed()
        {
            if (_disposed)
            {
                FileLog.Log($"Accessing disposed ClientSession (ID: {_id}, Ptr: 0x{_nativePointer.ToInt64():X})");
                throw new ObjectDisposedException(nameof(RoomData));
            }
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!_disposed)
            {
                if (_ownsNativeInstance && _nativePointer != IntPtr.Zero)
                {
                    ClientSessionNative.nexilis_client_session_destroy(_nativePointer);
                    _nativePointer = IntPtr.Zero;
                }
                _disposed = true;
            }
        }

        ~ClientSession()
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
