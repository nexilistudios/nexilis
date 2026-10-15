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
using System.Threading.Tasks;
using Nexilis.Util;

namespace Nexilis.Tests
{
    /// <summary>
    /// Verifies the connection lifecycle fails fast (throws) instead of
    /// hanging when the server is unreachable.
    /// </summary>
    public class NexilisConnectionTests
    {
        [DllImport("nexilisc", CallingConvention = CallingConvention.Cdecl)]
        static extern ushort nexilis_get_boost_tcp_port();

        [DllImport("nexilisc", CallingConvention = CallingConvention.Cdecl)]
        static extern void nexilis_set_boost_tcp_port(ushort port);

        public NexilisConnectionTests()
        {
            TestEnvironment.Initialize();
        }

        [Fact]
        public async Task ConnectAsync_Throws_WhenServerUnreachable()
        {
            // No Nexilis server is expected to be running on localhost during
            // tests, so the TCP connect must fail. The important regression
            // here is that it fails *fast*: previously the native connect loop
            // waited forever for initialization when the connection failed,
            // which froze the caller (e.g. Unity) instead of surfacing an error.
            var originalPort = nexilis_get_boost_tcp_port();
            nexilis_set_boost_tcp_port(55600);
            NexilisClient? client = null;

            try
            {
                client = new NexilisClient
                {
                    ServerAddress = "127.0.0.1",
                    Password = "password",
                };
                var connectTask = client.ConnectAsync();

                var completedTask = await Task.WhenAny(connectTask, Task.Delay(TimeSpan.FromSeconds(10)));
                Assert.True(completedTask == connectTask,
                    "ConnectAsync hung instead of failing when the server was unreachable.");

                await Assert.ThrowsAsync<InvalidOperationException>(async () => await connectTask);
            }
            finally
            {
                client?.Dispose();
                nexilis_set_boost_tcp_port(originalPort);
            }
        }
    }
}
