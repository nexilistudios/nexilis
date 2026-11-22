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

namespace Nexilis.Logger
{
    public class FunctionHandler : IDisposable
    {
        // Native function handler pointer
        private IntPtr _handlerPtr;
        private readonly ulong _handlerId;

        // Delegate for the callback function
        private readonly Action<LogLevel, string> _function;

        // Prevent GC of the native callback delegate
        private readonly FunctionHandlerNative.NativeLogCallback _nativeCallback;
        public static Action<LogLevel, string> EmptyFunction = (level, message) => {};
        public bool isEmpty => _function == EmptyFunction;

        // Constructor
        public FunctionHandler(Action<LogLevel, string> function)
        {
            if (function == null)
                throw new ArgumentNullException(nameof(function));

            _function = function;
            _nativeCallback = OnLogMessage;

            // Create the native function handler
            _handlerPtr = FunctionHandlerNative.nexilis_logger_FunctionHandler_create(_nativeCallback);
            if (_handlerPtr == IntPtr.Zero)
                throw new InvalidOperationException("Failed to create native function handler.");
            // The native logger takes ownership of the C++ handler when it is
            // added, leaving this wrapper with a null native handler pointer.
            _handlerId = FunctionHandlerNative.nexilis_logger_FunctionHandler_get_id(_handlerPtr);
        }

        /// <summary>
        /// Gets the native pointer to the file handler.
        /// </summary>
        public IntPtr HandlerPtr => _handlerPtr;

        // Callback function for native code
        private void OnLogMessage(LogLevel logLevel, IntPtr messagePtr)
        {
            string? message = Marshal.PtrToStringAnsi(messagePtr);
            if (message == null)
            {
                Console.WriteLine("Warning: Received null message from native code.");
                return;
            }
            _function(logLevel, message);
        }

        // Get the handler ID
        public ulong GetId()
        {
            if (_handlerPtr == IntPtr.Zero)
                throw new ObjectDisposedException(nameof(FunctionHandler));

            return _handlerId;
        }

        // Dispose pattern
        public void Dispose()
        {
            if (_handlerPtr != IntPtr.Zero)
            {
                FunctionHandlerNative.nexilis_logger_FunctionHandler_destroy(_handlerPtr);
                _handlerPtr = IntPtr.Zero;
            }
            GC.SuppressFinalize(this);
        }

        ~FunctionHandler()
        {
            Dispose();
        }
    }
}

