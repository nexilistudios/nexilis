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

namespace Nexilis.Tests
{
    /// <summary>
    /// One-time initialization required by managed wrappers before use:
    /// native interop main-thread tracking and per-class NxLogger setup.
    /// </summary>
    public static class TestEnvironment
    {
        static bool _initialized;
        static readonly object _sync = new object();

        public static void Initialize()
        {
            lock (_sync)
            {
                if (_initialized)
                {
                    return;
                }

                NativeInterop.InitializeMainThread();

                Action<Logger.LogLevel, string> noop = (_, _) => { };
                Vector3<float>.InitializeLogger(noop);
                Vector3<int>.InitializeLogger(noop);
                Vector3<ulong>.InitializeLogger(noop);
                NxData.InitializeLogger(noop);
                Packet.InitializeLogger(noop);
                _initialized = true;
            }
        }
    }
}
