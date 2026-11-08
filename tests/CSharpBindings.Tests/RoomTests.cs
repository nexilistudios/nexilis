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

using Xunit;
using Nexilis;
using Nexilis.Client;

namespace Nexilis.Tests
{
    public class RoomTests : IDisposable
    {
        private readonly ClientConfig _config;
        private readonly ClientAPI _clientApi;
        private readonly RoomData _roomData;
        private readonly Room _room;

        public RoomTests()
        {
            TestEnvironment.Initialize();
            _config = new ClientConfig();
            _clientApi = new ClientAPI(_config);
            _roomData = new RoomData(100, "TestRoom", RoomContext.ROOM_CONTEXT_3D, 10);
            _room = new Room(_roomData);
        }

        public void Dispose()
        {
            _room.Dispose();
            _roomData.Dispose();
            _clientApi.Dispose();
            _config.Dispose();
        }

        // The room takes ownership of added sessions (moved in natively),
        // so they must not be disposed by the caller afterwards.
        private ClientSession MakeSession(ulong id)
        {
            return new ClientSession(id, _clientApi.ClientApiPtr, false);
        }

        [Fact]
        public void CreateAndDestroy()
        {
            using var room = new Room(_roomData);
            Assert.Equal(0ul, room.GetClientAmount());
        }

        [Fact]
        public void GetId()
        {
            Assert.Equal(_roomData.Id, _room.GetId());
        }

        [Fact]
        public void GetName()
        {
            Assert.Equal("TestRoom", _room.GetName());
        }

        [Fact]
        public void GetContext()
        {
            Assert.Equal(RoomContext.ROOM_CONTEXT_3D, _room.GetContext());
        }

        [Fact]
        public void GetMaxSize()
        {
            Assert.Equal(10u, _room.GetMaxSize());
        }

        [Fact]
        public void GetCreatorId()
        {
            Assert.Equal(100ul, _room.GetCreatorId());
        }

        [Fact]
        public void MetadataMatchesRoomData()
        {
            // The room mirrors the RoomData it was created from.
            Assert.Equal(_roomData.Name, _room.GetName());
            Assert.Equal(_roomData.Context, _room.GetContext());
            Assert.Equal(_roomData.MaxSize, _room.GetMaxSize());
            Assert.Equal(_roomData.CreatorId, _room.GetCreatorId());
        }

        [Fact]
        public void MetadataAccessAfterDisposeThrows()
        {
            var room = new Room(_roomData);
            room.Dispose();

            Assert.Throws<ObjectDisposedException>(() => room.GetName());
            Assert.Throws<ObjectDisposedException>(() => room.GetContext());
            Assert.Throws<ObjectDisposedException>(() => room.GetMaxSize());
            Assert.Throws<ObjectDisposedException>(() => room.GetCreatorId());
            Assert.Throws<ObjectDisposedException>(() => room.GetId());
            Assert.Throws<ObjectDisposedException>(() => room.GetClientAmount());
        }

        [Fact]
        public void NonOwningRoomDoesNotFreeTheNativeRoom()
        {
            // A second, non-owning wrapper around the same handle (what a
            // RoomsCollection hands out) must not destroy the native room when
            // it is disposed, so the owning wrapper keeps working.
            using var roomData = new RoomData(1, "Owned", RoomContext.ROOM_CONTEXT_3D, 4);
            using var owner = new Room(roomData);

            var observer = new Room(owner.GetNativePointer(), ownsNativeInstance: false);
            observer.Dispose();

            Assert.Equal("Owned", owner.GetName());
        }

        [Fact]
        public void AddClient()
        {
            var client = MakeSession(10);
            _room.AddClient(client);

            Assert.Equal(1ul, _room.GetClientAmount());
        }

        [Fact]
        public void AddMultipleClients()
        {
            _room.AddClient(MakeSession(10));
            _room.AddClient(MakeSession(20));

            Assert.Equal(2ul, _room.GetClientAmount());
        }

        [Fact]
        public void RemoveClient()
        {
            _room.AddClient(MakeSession(10));
            _room.AddClient(MakeSession(20));

            _room.RemoveClient(10);

            Assert.Equal(1ul, _room.GetClientAmount());
        }

        [Fact]
        public void GetClients()
        {
            _room.AddClient(MakeSession(10));

            var clients = _room.GetClients();

            Assert.Single(clients);
            Assert.Equal(10ul, clients[0].GetId());
        }

        [Fact]
        public void GetClientsEmpty()
        {
            var clients = _room.GetClients();

            Assert.Empty(clients);
        }

        [Fact]
        public void ClientSnapshotsSurviveRoomChangesAndDisposal()
        {
            using var source = new ClientSession(10, _clientApi.ClientApiPtr, true);
            source.SetPosition3D(1, 2, 3);
            _room.AddClient(source);

            using var snapshot = _room.GetClients()[0];
            // Force room storage to grow, then remove the original client.
            for (ulong id = 20; id < 40; ++id)
            {
                using var added = new ClientSession(id, _clientApi.ClientApiPtr, true);
                _room.AddClient(added);
            }
            _room.RemoveClient(10);
            _room.Dispose();

            using var position = snapshot.GetPosition3D();
            Assert.Equal(1f, position.X);
            Assert.Equal(2f, position.Y);
            Assert.Equal(3f, position.Z);
            snapshot.SetUsername("snapshot");
            snapshot.SetPosition3D(4, 5, 6);
        }

        [Fact]
        public void DisposingSnapshotLeavesRoomClientIntact()
        {
            using var source = new ClientSession(10, _clientApi.ClientApiPtr, true);
            source.SetPosition3D(1, 2, 3);
            _room.AddClient(source);
            using (var snapshot = _room.GetClients()[0])
                snapshot.SetPosition3D(4, 5, 6);

            using var next = _room.GetClients()[0];
            using var position = next.GetPosition3D();
            Assert.Equal(1f, position.X);
            Assert.Equal(2f, position.Y);
            Assert.Equal(3f, position.Z);
        }

        [Fact]
        public void AddNullClientThrows()
        {
            Assert.Throws<System.ArgumentNullException>(() => _room.AddClient(null!));
        }

        // --- Communication ---

        [Fact]
        public void CommunicationCreateAndPayload()
        {
            var sender = MakeSession(10);
            using var comm = new Communication("hello world", sender);

            // Ids are generated at construction time.
            Assert.NotEqual(0ul, comm.GetId());
            Assert.Equal("hello world", comm.GetPayload());
        }

        [Fact]
        public void CommunicationCreateNullSenderThrows()
        {
            Assert.Throws<ArgumentNullException>(() => new Communication("test", null!));
        }

        [Fact]
        public void CommunicationCreateNullPayloadThrows()
        {
            var sender = MakeSession(10);
            Assert.Throws<ArgumentException>(() => new Communication(null!, sender));
        }

        [Fact]
        public void CommunicationAddMessage()
        {
            var sender = MakeSession(10);
            var comm = new Communication("test message", sender);

            _room.AddMessage(comm);

            Assert.NotEqual(0ul, comm.GetId());
            Assert.True(_room.ContainsCommunication(comm.GetId()));
        }

        [Fact]
        public void ContainsCommunication()
        {
            var sender = MakeSession(10);
            using var comm = new Communication("msg", sender);

            Assert.False(_room.ContainsCommunication(comm.GetId()));

            var added = new Communication("added message", sender);
            _room.AddMessage(added);
            // Check by id: object equality is unreliable after the
            // native move into the room.
            Assert.True(_room.ContainsCommunication(added.GetId()));
        }

        [Fact]
        public void GetMessages()
        {
            var sender = MakeSession(10);
            _room.AddMessage(new Communication("first", sender));
            _room.AddMessage(new Communication("second", sender));

            var messages = _room.GetMessages();

            Assert.Equal(2, messages.Count);

            // GetMessages returns copies that the caller owns.
            var payloads = new List<string>();
            foreach (var message in messages)
            {
                using (message)
                {
                    payloads.Add(message.GetPayload());
                }
            }
            payloads.Sort();
            Assert.Equal(new List<string> { "first", "second" }, payloads);
        }

        [Fact]
        public void GetMessagesEmpty()
        {
            Assert.Empty(_room.GetMessages());
        }

        [Fact]
        public void CommunicationDisposeIsIdempotent()
        {
            var sender = MakeSession(10);
            var comm = new Communication("test", sender);

            comm.Dispose();
            comm.Dispose();

            Assert.Throws<ObjectDisposedException>(() => comm.GetPayload());
        }
    }
}
