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
    public class RoomDataTests : IDisposable
    {
        private readonly RoomData _roomData;

        public RoomDataTests()
        {
            _roomData = new RoomData(100, "TestRoom", RoomContext.ROOM_CONTEXT_2D, 10);
        }

        public void Dispose()
        {
            _roomData.Dispose();
        }

        [Fact]
        public void CreateWithAllParams()
        {
            using var room = new RoomData(100, "TestRoom", RoomContext.ROOM_CONTEXT_2D, 10);

            Assert.Equal(100ul, room.CreatorId);
            Assert.Equal("TestRoom", room.Name);
            Assert.Equal(RoomContext.ROOM_CONTEXT_2D, room.Context);
            Assert.Equal(10u, room.MaxSize);
            Assert.NotEqual(0ul, room.Id);
        }

        [Fact]
        public void CreateWith3DContext()
        {
            using var room = new RoomData(1, "MyRoom", RoomContext.ROOM_CONTEXT_3D, 20);

            Assert.Equal(1ul, room.CreatorId);
            Assert.Equal("MyRoom", room.Name);
            Assert.Equal(RoomContext.ROOM_CONTEXT_3D, room.Context);
            Assert.Equal(20u, room.MaxSize);
        }

        [Fact]
        public void Utf8NameRoundTrips()
        {
            using var room = new RoomData(1, "Tähtihuone", RoomContext.ROOM_CONTEXT_3D, 20);
            Assert.Equal("Tähtihuone", room.Name);
        }

        [Fact]
        public void CreateWithEmptyNameThrows()
        {
            Assert.Throws<ArgumentException>(() => new RoomData(0, "", RoomContext.ROOM_CONTEXT_2D, 10));
        }

        [Fact]
        public void UniqueIds()
        {
            using var a = new RoomData(0, "RoomA", RoomContext.ROOM_CONTEXT_2D, 10);
            using var b = new RoomData(0, "RoomB", RoomContext.ROOM_CONTEXT_3D, 5);

            Assert.NotEqual(a.Id, b.Id);
        }

        [Fact]
        public void ContextEnumValues()
        {
            using var room2D = new RoomData(0, "R2D", RoomContext.ROOM_CONTEXT_2D, 10);
            using var room3D = new RoomData(0, "R3D", RoomContext.ROOM_CONTEXT_3D, 10);

            Assert.Equal(RoomContext.ROOM_CONTEXT_2D, room2D.Context);
            Assert.Equal(RoomContext.ROOM_CONTEXT_3D, room3D.Context);
        }

        [Fact]
        public void AccessAfterDisposeThrows()
        {
            _roomData.Dispose();
            Assert.Throws<ObjectDisposedException>(() => _roomData.Name);
        }
    }
}
