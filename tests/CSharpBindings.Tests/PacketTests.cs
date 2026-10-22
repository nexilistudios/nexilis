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
using Xunit;
using Nexilis;
using Nexilis.Client;

namespace Nexilis.Tests
{
    public class PacketTests : IDisposable
    {
        private readonly ClientConfig _config;
        private readonly ClientAPI _clientApi;

        public PacketTests()
        {
            TestEnvironment.Initialize();
            _config = new ClientConfig();
            _clientApi = new ClientAPI(_config);
        }

        public void Dispose()
        {
            _clientApi.Dispose();
            _config.Dispose();
        }

        [Fact]
        public void GetGeneralClientId()
        {
            using var packet = Packet.GetGeneralClientId(_clientApi.ClientApiPtr);

            Assert.True(packet.Size > 0);
            Assert.NotEqual(IntPtr.Zero, packet.DataPointer);
        }

        [Fact]
        public void GetInfoRooms()
        {
            using var packet = Packet.InfoRooms(_clientApi.ClientApiPtr);

            Assert.True(packet.Size > 0);
            Assert.NotEqual(IntPtr.Zero, packet.DataPointer);
        }

        [Fact]
        public void GetInfoGeneral()
        {
            using var packet = Packet.InfoGeneral(_clientApi.ClientApiPtr);

            Assert.True(packet.Size > 0);
        }

        [Fact]
        public void GetInfoClients()
        {
            using var packet = Packet.InfoClients(_clientApi.ClientApiPtr);

            Assert.True(packet.Size > 0);
        }

        [Fact]
        public void RoomManagementLeave()
        {
            using var packet = Packet.RoomManagementLeave(_clientApi.ClientApiPtr);

            Assert.True(packet.Size > 0);
        }

        [Fact]
        public void RoomManagementJoin()
        {
            using var packet = Packet.RoomManagementJoin(_clientApi.ClientApiPtr, 42);

            Assert.True(packet.Size > 0);
        }

        [Fact]
        public void RoomManagementCreate3D()
        {
            using var packet = Packet.RoomManagementCreate(_clientApi.ClientApiPtr, RoomContext.ROOM_CONTEXT_3D, "NewRoom");

            Assert.True(packet.Size > 0);
        }

        [Fact]
        public void RoomManagementCreate2D()
        {
            using var packet = Packet.RoomManagementCreate(_clientApi.ClientApiPtr, RoomContext.ROOM_CONTEXT_2D, "FlatRoom");

            Assert.True(packet.Size > 0);
        }

        [Fact]
        public void Player3DPositionDirect()
        {
            using var packet = Packet.Player3DPositionDirect(_clientApi.ClientApiPtr, 1.0f, 2.0f, 3.0f);

            Assert.True(packet.Size > 0);
        }

        [Fact]
        public void Player3DPositionFromVector()
        {
            using var position = new Vector3<float>(10.0f, 20.0f, 30.0f);
            using var packet = Packet.Player3DPosition(_clientApi.ClientApiPtr, position);

            Assert.True(packet.Size > 0);
        }

        [Fact]
        public void Player3DDimension()
        {
            using var dimensions = new Vector3<float>(1.0f, 1.0f, 1.0f);
            using var packet = Packet.Player3DDimension(_clientApi.ClientApiPtr, dimensions);

            Assert.True(packet.Size > 0);
        }

        [Fact]
        public void Player3DMovementDirect()
        {
            using var packet = Packet.Player3DMovementDirect(_clientApi.ClientApiPtr, 0.1f, 0.2f, 0.3f, 0.016f);

            Assert.True(packet.Size > 0);
        }

        [Fact]
        public void Player3DMovementFromVector()
        {
            using var movement = new Vector3<float>(0.5f, 0.0f, -0.5f);
            using var packet = Packet.Player3DMovement(_clientApi.ClientApiPtr, movement, 0.033f);

            Assert.True(packet.Size > 0);
        }

        [Fact]
        public void Player3DFire()
        {
            using var packet = Packet.Player3DFire(_clientApi.ClientApiPtr, 0.0f, 0.0f, -1.0f, 1);

            Assert.True(packet.Size == 17);
        }
    }
}
