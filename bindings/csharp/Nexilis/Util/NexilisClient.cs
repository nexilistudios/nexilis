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
using System.Linq;
using System.Threading.Tasks;
using Nexilis.Client;

namespace Nexilis.Util
{
    /// <summary>
    /// High level, engine-agnostic client for the Nexilis bindings. Wraps the
    /// full connection lifecycle (config, BoostTCP / BoostUDP clients, room
    /// join) and exposes the connected room as plain data through callbacks,
    /// so it can be driven from any game loop.
    ///
    /// Wire the callbacks from your engine layer with anonymous functions,
    /// e.g. in Unity:
    /// <code>
    /// client = new NexilisClient
    /// {
    ///     ServerAddress = "127.0.0.1",
    ///     LogHandler = (level, msg) => Debug.Log($"[Nexilis] {msg}"),
    /// };
    /// client.OnReady += () => Debug.Log("Connected!");
    /// client.OnRemoteClientPosition += (id, pos) => remoteGo.transform.position = new Vector3(pos.X, pos.Y, pos.Z);
    /// await client.ConnectAsync();
    /// </code>
    /// There is no Unity code in this class on purpose.
    /// </summary>
    public class NexilisClient : IDisposable
    {
        /// <summary>
        /// Used when no <see cref="LogHandler"/> is set. Only warnings and
        /// worse reach the console so the library stays quiet by default.
        /// The log level is fully qualified because the "Logger" property of
        /// this class hides the Nexilis.Logger namespace inside instance
        /// members.
        /// </summary>
        static readonly Action<Nexilis.Logger.LogLevel, string> DefaultLogHandler = (level, message) =>
        {
            if (level >= Nexilis.Logger.LogLevel.WARNING)
            {
                Console.Error.WriteLine($"[Nexilis] {message}");
            }
        };

        /// <summary>Server address, e.g. "127.0.0.1". Set before <see cref="ConnectAsync"/>.</summary>
        public string ServerAddress { get; set; } = "127.0.0.1";

        /// <summary>Server password. Set before <see cref="ConnectAsync"/>.</summary>
        public string Password { get; set; } = "password";

        /// <summary>Authentication mode (0 = empty, 1 = skip, 2 = password_protected).</summary>
        public int AuthenticationMode { get; set; } = 2;

        /// <summary>
        /// When set, <see cref="ConnectAsync"/> joins this room instead of
        /// picking one automatically. Ignored when the room is unknown.
        /// </summary>
        public ulong? PreferredRoomId { get; set; }

        /// <summary>
        /// Whether <see cref="ConnectAsync"/> joins a room when it is done
        /// connecting. Disable to browse rooms with
        /// <see cref="GetAvailableRooms"/> first and join later with
        /// <see cref="JoinRoomAsync"/>.
        /// </summary>
        public bool AutoJoinOnConnect { get; set; } = true;

        /// <summary>How long a join or leave request is awaited, in milliseconds.</summary>
        public int RoomJoinTimeoutMs { get; set; } = 5000;

        /// <summary>How long the poll loop waits between attempts, in milliseconds.</summary>
        public int RoomPollIntervalMs { get; set; } = 50;

        /// <summary>
        /// Routes the internal logger. Unity: <c>(level, msg) => Debug.Log(msg)</c>.
        /// Defaults to warnings-and-worse on the console.
        /// </summary>
        public Action<Logger.LogLevel, string>? LogHandler { get; set; }

        /// <summary>Fired once the client joined a room and is ready to play.</summary>
        public Action? OnReady { get; set; }

        /// <summary>Fired when the client has been torn down.</summary>
        public Action? OnDisconnected { get; set; }

        /// <summary>Fired after the client successfully left its room.</summary>
        public Action? OnLeftRoom { get; set; }

        /// <summary>Fired when another client joins the room.</summary>
        public Action<ulong>? OnRemoteClientAdded { get; set; }

        /// <summary>Fired when another client leaves the room.</summary>
        public Action<ulong>? OnRemoteClientRemoved { get; set; }

        /// <summary>Fired with the latest position of a remote client. Raised from <see cref="Update"/>.</summary>
        public Action<ulong, Position>? OnRemoteClientPosition { get; set; }

        /// <summary>True once the client joined a room and is ready.</summary>
        public bool IsReady { get; private set; }

        /// <summary>True once <see cref="Disconnect"/> or <see cref="Dispose"/> has run.</summary>
        public bool IsDisposed => _disposed;

        /// <summary>The unique id of this client. Valid after <see cref="OnReady"/>.</summary>
        public ulong ClientId { get; private set; }

        /// <summary>The id of the room this client is in. Zero until connected.</summary>
        public ulong ClientRoomId { get; private set; }

        /// <summary>The room this client is in. Valid after <see cref="OnReady"/>.</summary>
        public Room? ClientRoom { get; private set; }

        /// <summary>
        /// A snapshot of the metadata of the room this client is in. Stays
        /// valid after leaving the room, unlike <see cref="ClientRoom"/>.
        /// </summary>
        public RoomInfo? ClientRoomInfo { get; private set; }

        /// <summary>The client API handle (native pointer wrapper).</summary>
        public ClientAPI? ClientApi { get; private set; }

        /// <summary>Shared logger instance.</summary>
        public NxLogger? Logger { get; private set; }

        /// <summary>Protocol manager (handles packet routing).</summary>
        public ProtocolManager? ProtocolManager { get; private set; }

        /// <summary>TCP transport used for reliable room/player management packets.</summary>
        public BoostTCPClient? TcpClient { get; private set; }

        /// <summary>UDP transport used for latency sensitive packets (positions, movement).</summary>
        public BoostUDPClient? UdpClient { get; private set; }

        /// <summary>The amount of remote clients currently tracked by <see cref="Update"/>.</summary>
        public int RemoteClientCount => _remoteClients.Count;

        readonly HashSet<ulong> _remoteClients = new HashSet<ulong>();
        bool _disposed;

        /// <summary>
        /// Connects to the server, joins a room and becomes ready. Returns true
        /// on success. Throws when the server cannot be reached.
        /// </summary>
        /// <remarks>
        /// Unless <see cref="AutoJoinOnConnect"/> is disabled the client joins
        /// <see cref="PreferredRoomId"/> or, when that is not set, the first
        /// room with free space.
        /// </remarks>
        public async Task<bool> ConnectAsync()
        {
            ThrowIfDisposed();

            NativeInterop.InitializeMainThread();

            Logger = new NxLogger("NexilisClient");
            Logger.Setup(LogHandler ?? DefaultLogHandler);

            ProtocolManager = new ProtocolManager();

            var clientConfig = new ClientConfig();
            clientConfig.SetBoostTCPAddress(ServerAddress);
            clientConfig.SetBoostUDP(ServerAddress);
            clientConfig.SetPassword(Password);
            clientConfig.SetMode(AuthenticationMode);

            ClientApi = new ClientAPI(clientConfig);
            TcpClient = new BoostTCPClient(ProtocolManager, ClientApi);
            UdpClient = new BoostUDPClient(ProtocolManager, ClientApi);

            Logger.Info("Starting BoostTCP client...");
            bool tcpStarted = await Task.Run(() => TcpClient!.StartClient(ClientApi!));
            if (!tcpStarted)
            {
                throw new InvalidOperationException("BoostTCP client failed to start.");
            }
            Logger.Info("BoostTCP client started.");

            Logger.Info("Starting BoostUDP client...");
            await Task.Run(() => UdpClient!.Start());
            if (!UdpClient!.IsConnected())
            {
                throw new InvalidOperationException("BoostUDP client failed to start.");
            }
            Logger.Info("BoostUDP client started.");

            await WaitForRoomsCreatedAsync();

            if (AutoJoinOnConnect)
            {
                await JoinAnyRoomAsync();
            }

            return IsReady;
        }

        async Task WaitForRoomsCreatedAsync()
        {
            using (var waiter = new Waiter(ClientApi!.ClientApiPtr))
            {
                await waiter.WaitUntilRoomsCreated();
            }
            Logger!.Info("Rooms verified and ready.");
        }

        /// <summary>
        /// Returns every room the server currently exposes, as plain
        /// <see cref="RoomInfo"/> snapshots that stay valid after the call.
        /// </summary>
        /// <exception cref="ObjectDisposedException">Thrown when disconnected.</exception>
        /// <exception cref="InvalidOperationException">Thrown when not connected.</exception>
        public IReadOnlyList<RoomInfo> GetAvailableRooms()
        {
            ThrowIfNotConnected();
            return ClientApi!.GetActiveRoomInfos();
        }

        /// <summary>
        /// Returns every room the server currently exposes, without blocking
        /// the calling thread. Useful from a UI thread while the game loop
        /// keeps running.
        /// </summary>
        /// <exception cref="ObjectDisposedException">Thrown when disconnected.</exception>
        /// <exception cref="InvalidOperationException">Thrown when not connected.</exception>
        public async Task<IReadOnlyList<RoomInfo>> GetAvailableRoomsAsync()
        {
            ThrowIfNotConnected();

            var clientApi = ClientApi!;
            return await Task.Run(() => clientApi.GetActiveRoomInfos()).ConfigureAwait(false);
        }

        /// <summary>Amount of rooms the server currently exposes.</summary>
        /// <exception cref="ObjectDisposedException">Thrown when disconnected.</exception>
        /// <exception cref="InvalidOperationException">Thrown when not connected.</exception>
        public int GetAvailableRoomCount()
        {
            ThrowIfNotConnected();
            return (int)ClientApi!.GetActiveRoomsCount();
        }

        /// <summary>
        /// Looks a single room up by id. Returns false when the server does not
        /// expose a room with that id.
        /// </summary>
        public bool TryGetRoomInfo(ulong roomId, out RoomInfo? roomInfo)
        {
            ThrowIfNotConnected();

            roomInfo = null;
            foreach (var info in ClientApi!.GetActiveRoomInfos())
            {
                if (info.Id != roomId)
                {
                    continue;
                }

                roomInfo = info;
                return true;
            }
            return false;
        }

        /// <summary>
        /// Joins a specific room. Returns true once the server confirms the
        /// placement, false when the room could not be joined in time.
        /// </summary>
        /// <exception cref="ObjectDisposedException">Thrown when disconnected.</exception>
        /// <exception cref="InvalidOperationException">Thrown when not connected.</exception>
        public async Task<bool> JoinRoomAsync(ulong roomId)
        {
            if (roomId == 0)
            {
                throw new ArgumentException("Room id must not be zero.", nameof(roomId));
            }

            ThrowIfNotConnected();

            if (ClientRoomId == roomId && IsReady)
            {
                Logger!.Info($"Already in room {roomId}.");
                return true;
            }

            Logger!.Info($"Joining room {roomId}...");
            using (var joinPacket = Packet.RoomManagementJoin(ClientApi!.ClientApiPtr, roomId))
            {
                SendPacket(joinPacket, PacketTransport.Tcp);
            }

            bool joined = await WaitUntil(() => ClientApi!.ClientRoomId() == roomId, RoomJoinTimeoutMs);
            if (!joined)
            {
                Logger!.Warning($"Timed out joining room {roomId}.");
                return false;
            }

            EnterRoom();
            return true;
        }

        /// <summary>
        /// Joins <see cref="PreferredRoomId"/> or, when that is unset, the
        /// first room that still has free space. Falls back to the newest room
        /// when every room is full.
        /// </summary>
        /// <exception cref="InvalidOperationException">Thrown when the server exposes no rooms.</exception>
        public async Task<bool> JoinAnyRoomAsync()
        {
            ThrowIfNotConnected();

            var rooms = GetAvailableRooms();
            if (rooms.Count == 0)
            {
                throw new InvalidOperationException("No active rooms available to join.");
            }

            ulong roomId = SelectRoomToJoin(rooms);
            if (roomId == 0)
            {
                throw new InvalidOperationException("No active rooms found.");
            }

            return await JoinRoomAsync(roomId);
        }

        /// <summary>
        /// Picks the room to join: the preferred one when it exists, otherwise
        /// the first room with free space, otherwise the last room.
        /// </summary>
        ulong SelectRoomToJoin(IReadOnlyList<RoomInfo> rooms)
        {
            if (PreferredRoomId.HasValue)
            {
                foreach (var room in rooms)
                {
                    if (room.Id == PreferredRoomId.Value)
                    {
                        Logger!.Info($"Using preferred room {room.Id}.");
                        return room.Id;
                    }
                }
                Logger!.Warning($"Preferred room {PreferredRoomId.Value} is not available, picking another one.");
            }

            foreach (var room in rooms)
            {
                if (room.HasFreeSpace)
                {
                    return room.Id;
                }
            }

            // Everything is full, join the newest one and let the server decide.
            return rooms[rooms.Count - 1].Id;
        }

        /// <summary>
        /// Leaves the current room. Returns false when the client was not in a
        /// room or the server did not confirm in time.
        /// </summary>
        /// <exception cref="ObjectDisposedException">Thrown when disconnected.</exception>
        /// <exception cref="InvalidOperationException">Thrown when not connected.</exception>
        public async Task<bool> LeaveRoomAsync()
        {
            ThrowIfNotConnected();

            if (ClientRoomId == 0)
            {
                return false;
            }

            ulong leavingRoomId = ClientRoomId;
            Logger!.Info($"Leaving room {leavingRoomId}...");
            using (var leavePacket = Packet.RoomManagementLeave(ClientApi!.ClientApiPtr))
            {
                SendPacket(leavePacket, PacketTransport.Tcp);
            }

            bool left = await WaitUntil(() => ClientApi!.ClientRoomId() == 0, RoomJoinTimeoutMs);
            if (!left)
            {
                Logger.Warning($"Timed out leaving room {leavingRoomId}.");
                return false;
            }

            ExitRoom();
            Logger.Info($"Left room {leavingRoomId}.");
            TryInvoke(OnLeftRoom);
            return true;
        }

        /// <summary>
        /// Asks the server to create a room and then joins it. The server does
        /// not place the creator in the room, so the new room is looked up by
        /// id and joined explicitly. Returns true once the client has been
        /// placed in the new room.
        /// </summary>
        /// <param name="roomName">Name of the new room.</param>
        /// <param name="context">2D or 3D context of the new room.</param>
        /// <exception cref="ArgumentException">Thrown when the room name is empty.</exception>
        /// <exception cref="ObjectDisposedException">Thrown when disconnected.</exception>
        /// <exception cref="InvalidOperationException">Thrown when not connected.</exception>
        public async Task<bool> CreateRoomAsync(string roomName, RoomContext context = RoomContext.ROOM_CONTEXT_3D)
        {
            if (string.IsNullOrWhiteSpace(roomName))
            {
                throw new ArgumentException("Room name must not be empty.", nameof(roomName));
            }

            ThrowIfNotConnected();

            var clientApi = ClientApi!;
            var knownRoomIds = new HashSet<ulong>();
            foreach (var room in clientApi.GetActiveRoomInfos())
            {
                knownRoomIds.Add(room.Id);
            }

            Logger!.Info($"Creating room '{roomName}'...");
            using (var createPacket = Packet.RoomManagementCreate(clientApi.ClientApiPtr, context, roomName))
            {
                SendPacket(createPacket, PacketTransport.Tcp);
            }

            ulong newRoomId = 0;
            bool appeared = await WaitUntil(() => FindCreatedRoom(knownRoomIds, out newRoomId), RoomJoinTimeoutMs);
            if (!appeared || newRoomId == 0)
            {
                Logger.Warning($"Timed out waiting for the created room '{roomName}'.");
                return false;
            }

            Logger.Info($"Created room {newRoomId}, joining it...");
            return await JoinRoomAsync(newRoomId).ConfigureAwait(false);
        }

        /// <summary>
        /// Looks for a room that appeared after the create request was sent.
        /// A room created by this client is preferred over a room somebody
        /// else created in the meantime.
        /// </summary>
        bool FindCreatedRoom(HashSet<ulong> knownRoomIds, out ulong roomId)
        {
            roomId = 0;
            ulong myClientId = ClientApi!.GetClientId();
            ulong fallbackId = 0;

            foreach (var room in ClientApi!.GetActiveRoomInfos())
            {
                if (knownRoomIds.Contains(room.Id))
                {
                    continue;
                }

                if (myClientId != 0 && room.CreatorId == myClientId)
                {
                    roomId = room.Id;
                    return true;
                }

                if (fallbackId == 0)
                {
                    fallbackId = room.Id;
                }
            }

            if (fallbackId == 0)
            {
                return false;
            }

            Logger!.Info($"Room {fallbackId} appeared without a creator id, assuming it is the room we created.");
            roomId = fallbackId;
            return true;
        }

        /// <summary>
        /// Polls <paramref name="condition"/> until it is true or
        /// <paramref name="timeoutMs"/> has elapsed.
        /// </summary>
        async Task<bool> WaitUntil(Func<bool> condition, int timeoutMs)
        {
            int interval = Math.Max(1, RoomPollIntervalMs);
            int waited = 0;

            while (waited < timeoutMs)
            {
                if (condition())
                {
                    return true;
                }

                await Task.Delay(interval).ConfigureAwait(false);
                waited += interval;
            }

            return condition();
        }

        /// <summary>
        /// Caches the room the client has been placed in and raises
        /// <see cref="OnReady"/>.
        /// </summary>
        void EnterRoom()
        {
            ClientRoomId = ClientApi!.ClientRoomId();
            ClientId = ClientApi.GetClientId();

            // The previous room wrapper is a native handle of our own, drop it
            // before replacing it. The room it wrapped is owned by the
            // collection, not by us, so only the wrapper is released.
            SafeDispose(ClientRoom);
            ClientRoom = ClientApi.GetRoomFromId(ClientRoomId);
            ClientRoomInfo = ClientRoom != null ? RoomInfo.FromRoom(ClientRoom) : null;

            Logger!.Info($"Ready. ClientId={ClientId}, room {ClientRoomId} has {ClientRoom?.GetClientAmount() ?? 0} clients.");
            IsReady = true;
            TryInvoke(OnReady);
        }

        /// <summary>
        /// Drops the cached room state. The remote client set is left alone so
        /// <see cref="OnRemoteClientRemoved"/> is not raised for a deliberate
        /// leave.
        /// </summary>
        void ExitRoom()
        {
            IsReady = false;
            ClientRoomId = 0;
            ClientRoomInfo = null;

            SafeDispose(ClientRoom);
            ClientRoom = null;
        }

        /// <summary>
        /// Polls the room and raises the remote-client callbacks. Call this
        /// once per frame / tick from your game loop (Unity: MonoBehaviour.Update).
        /// </summary>
        public void Update()
        {
            if (_disposed || !IsReady || ClientRoom == null) return;

            var roomClients = ClientRoom.GetClients();
            var present = new HashSet<ulong>();

            foreach (var roomClient in roomClients)
            {
                var id = roomClient.GetId();
                if (id == 0 || id == ClientId) continue;

                present.Add(id);
                if (_remoteClients.Add(id))
                {
                    Logger!.Info($"Remote client added: {id}");
                    TryInvoke(OnRemoteClientAdded, id);
                }
            }

            foreach (var id in _remoteClients.Where(id => !present.Contains(id)).ToList())
            {
                _remoteClients.Remove(id);
                Logger!.Info($"Remote client removed: {id}");
                TryInvoke(OnRemoteClientRemoved, id);
            }

            foreach (var roomClient in roomClients)
            {
                var id = roomClient.GetId();
                if (id == 0 || id == ClientId || !_remoteClients.Contains(id)) continue;

                if (TryGetPosition(roomClient, out var position))
                {
                    TryInvoke(OnRemoteClientPosition, id, position);
                }
            }
        }

        /// <summary>
        /// Ids of the remote clients currently tracked by <see cref="Update"/>.
        /// A snapshot, safe to iterate while the game loop keeps running.
        /// </summary>
        public IReadOnlyList<ulong> GetRemoteClientIds()
        {
            return _remoteClients.ToArray();
        }

        /// <summary>
        /// Looks a remote client session up. Returns null when the client is
        /// not in any of the rooms this client knows about.
        /// </summary>
        public ClientSession? GetRemoteClient(ulong clientId)
        {
            ThrowIfNotConnected();

            try
            {
                return ClientApi!.GetClientFromClientId(clientId);
            }
            catch (InvalidOperationException)
            {
                return null;
            }
        }

        /// <summary>
        /// Gets the username the server has for a client, or an empty string
        /// when the client is unknown.
        /// </summary>
        public string GetClientUsername(ulong clientId)
        {
            ThrowIfNotConnected();
            return ClientApi!.GetClientUsername(clientId);
        }

        /// <summary>
        /// Sends the player position over UDP.
        /// </summary>
        public void SendPosition(float x, float y, float z)
        {
            ThrowIfNotReady();
            using (var packet = Packet.Player3DPositionDirect(ClientApi!.ClientApiPtr, x, y, z))
            {
                SendPacket(packet, PacketTransport.Udp);
            }
        }

        public void SetTeam(string team)
        {
            ThrowIfNotReady();
            using (var packet = Packet.Player3DSetTeam(ClientApi!.ClientApiPtr, team))
                SendPacket(packet, PacketTransport.Tcp);
        }

        [Obsolete("Servers reject client-selected target and damage. Use Fire instead.")]
        public void Shoot(ulong targetId, float damage)
        {
            ThrowIfNotReady();
            using (var packet = Packet.Player3DShoot(ClientApi!.ClientApiPtr, targetId, damage))
                SendPacket(packet, PacketTransport.Tcp);
        }

        /// <summary>Sends aim direction and weapon slot for server-side hit resolution.</summary>
        public void Fire(float x, float y, float z, byte slot)
        {
            ThrowIfNotReady();
            using (var packet = Packet.Player3DFire(ClientApi!.ClientApiPtr, x, y, z, slot))
                SendPacket(packet, PacketTransport.Tcp);
        }

        /// <summary>Relays a positional sound event to players in the room.</summary>
        public void SendAudioEvent(byte sound, float x, float y, float z)
        {
            ThrowIfNotReady();
            using (var packet = Packet.Player3DAudioEvent(ClientApi!.ClientApiPtr, sound, x, y, z))
                SendPacket(packet, PacketTransport.Udp);
        }

        public void Broadcast(string message)
        {
            ThrowIfNotReady();
            using (var packet = Packet.RoomBroadcast(ClientApi!.ClientApiPtr, message))
                SendPacket(packet, PacketTransport.Tcp);
        }

        /// <summary>
        /// Sends the player position over UDP.
        /// </summary>
        public void SendPosition(Position position)
        {
            SendPosition(position.X, position.Y, position.Z);
        }

        /// <summary>
        /// Sends a player movement delta over UDP, which also updates the
        /// position on the server.
        /// </summary>
        public void SendMovement(float x, float y, float z, float deltaTime)
        {
            ThrowIfNotReady();
            using (var packet = Packet.Player3DMovementDirect(ClientApi!.ClientApiPtr, x, y, z, deltaTime))
            {
                SendPacket(packet, PacketTransport.Udp);
            }
        }

        /// <summary>
        /// Sends a player movement delta over UDP, which also updates the
        /// position on the server.
        /// </summary>
        public void SendMovement(Position movement, float deltaTime)
        {
            SendMovement(movement.X, movement.Y, movement.Z, deltaTime);
        }

        /// <summary>
        /// Sends a whole package over the requested transport.
        /// </summary>
        /// <param name="data">The serialized package, e.g. from a <c>Packet</c> factory.</param>
        /// <param name="transport">TCP for reliable data, UDP for latency sensitive data.</param>
        /// <returns>True when the package was handed to the transport.</returns>
        /// <remarks>The caller keeps ownership of <paramref name="data"/> and must dispose it.</remarks>
        /// <exception cref="ArgumentNullException">Thrown when data is null.</exception>
        /// <exception cref="ObjectDisposedException">Thrown when disconnected.</exception>
        /// <exception cref="InvalidOperationException">Thrown when not connected.</exception>
        public bool SendPacket(NxData data, PacketTransport transport)
        {
            if (data == null)
            {
                throw new ArgumentNullException(nameof(data));
            }

            ThrowIfNotConnected();
            return SendBytes(data.ToBytes(), transport);
        }

        /// <summary>
        /// Sends a whole package over the requested transport.
        /// </summary>
        /// <param name="data">The serialized package bytes.</param>
        /// <param name="transport">TCP for reliable data, UDP for latency sensitive data.</param>
        /// <returns>True when the package was handed to the transport.</returns>
        /// <exception cref="ArgumentNullException">Thrown when data is null.</exception>
        /// <exception cref="ArgumentException">Thrown when data is empty.</exception>
        /// <exception cref="ObjectDisposedException">Thrown when disconnected.</exception>
        /// <exception cref="InvalidOperationException">Thrown when not connected.</exception>
        public bool SendPacket(byte[] data, PacketTransport transport)
        {
            if (data == null)
            {
                throw new ArgumentNullException(nameof(data));
            }
            if (data.Length == 0)
            {
                throw new ArgumentException("Package must not be empty.", nameof(data));
            }

            ThrowIfNotConnected();
            return SendBytes(data, transport);
        }

        /// <summary>Sends a whole package over the reliable TCP transport.</summary>
        /// <inheritdoc cref="SendPacket(NxData, PacketTransport)" path="/exception"/>
        public bool SendPacketTcp(NxData data) => SendPacket(data, PacketTransport.Tcp);

        /// <summary>Sends a whole package over the unreliable UDP transport.</summary>
        /// <inheritdoc cref="SendPacket(NxData, PacketTransport)" path="/exception"/>
        public bool SendPacketUdp(NxData data) => SendPacket(data, PacketTransport.Udp);

        /// <summary>Sends a whole package over the reliable TCP transport.</summary>
        /// <inheritdoc cref="SendPacket(byte[], PacketTransport)" path="/exception"/>
        public bool SendPacketTcp(byte[] data) => SendPacket(data, PacketTransport.Tcp);

        /// <summary>Sends a whole package over the unreliable UDP transport.</summary>
        /// <inheritdoc cref="SendPacket(byte[], PacketTransport)" path="/exception"/>
        public bool SendPacketUdp(byte[] data) => SendPacket(data, PacketTransport.Udp);

        /// <summary>
        /// Stops the clients and releases all native resources. Safe to call
        /// multiple times.
        /// </summary>
        public void Disconnect()
        {
            if (_disposed) return;
            _disposed = true;

            IsReady = false;

            SafeDispose(UdpClient);
            SafeDispose(TcpClient);
            SafeDispose(ClientRoom);
            SafeDispose(ClientApi);
            SafeDispose(ProtocolManager);
            SafeDispose(Logger);

            UdpClient = null;
            TcpClient = null;
            ClientApi = null;
            ProtocolManager = null;
            Logger = null;
            ClientRoom = null;
            ClientRoomInfo = null;
            ClientId = 0;
            ClientRoomId = 0;

            _remoteClients.Clear();
            TryInvoke(OnDisconnected);
        }

        public void Dispose() => Disconnect();

        bool SendBytes(byte[] bytes, PacketTransport transport)
        {
            switch (transport)
            {
                case PacketTransport.Tcp:
                    SendTcpBytes(bytes);
                    return true;
                case PacketTransport.Udp:
                    SendUdpBytes(bytes);
                    return true;
                default:
                    throw new ArgumentOutOfRangeException(nameof(transport), transport, "Unknown transport.");
            }
        }

        void SendTcpBytes(byte[] bytes)
        {
            var tcp = TcpClient!;
            var length = (uint)bytes.Length;
            NativeInterop.ExecuteSafe(() => tcp.SendMessage(bytes, length), "SendPacketTcp");
        }

        void SendUdpBytes(byte[] bytes)
        {
            var udp = UdpClient!;
            var length = (uint)bytes.Length;
            NativeInterop.ExecuteSafe(() => udp.SendMessage(bytes, length), "SendPacketUdp");
        }

        static bool TryGetPosition(ClientSession session, out Position position)
        {
            position = default;
            if (!ClientSessionNative.nexilis_client_session_get_position_3D_values(
                    session.GetNativePointer(), out float x, out float y, out float z))
            {
                return false;
            }

            position = new Position(x, y, z);
            return true;
        }

        void ThrowIfDisposed()
        {
            if (_disposed) throw new ObjectDisposedException(nameof(NexilisClient));
        }

        void ThrowIfNotConnected()
        {
            ThrowIfDisposed();
            if (ClientApi == null || TcpClient == null || UdpClient == null)
            {
                throw new InvalidOperationException("NexilisClient is not connected. Call ConnectAsync first.");
            }
        }

        void ThrowIfNotReady()
        {
            ThrowIfNotConnected();
            if (!IsReady) throw new InvalidOperationException("NexilisClient has not joined a room yet.");
        }

        void TryInvoke(Action? callback)
        {
            if (callback == null) return;
            try
            {
                callback();
            }
            catch (Exception ex)
            {
                Logger?.Error($"Exception in callback: {ex.Message}");
            }
        }

        void TryInvoke<T>(Action<T>? callback, T value)
        {
            if (callback == null) return;
            try
            {
                callback(value);
            }
            catch (Exception ex)
            {
                Logger?.Error($"Exception in callback: {ex.Message}");
            }
        }

        void TryInvoke<T1, T2>(Action<T1, T2>? callback, T1 value1, T2 value2)
        {
            if (callback == null) return;
            try
            {
                callback(value1, value2);
            }
            catch (Exception ex)
            {
                Logger?.Error($"Exception in callback: {ex.Message}");
            }
        }

        static void SafeDispose(IDisposable? disposable)
        {
            try
            {
                disposable?.Dispose();
            }
            catch
            {
                // Best effort teardown.
            }
        }
    }
}
