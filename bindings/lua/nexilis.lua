-- Copyright (C) 2026 Valtteri Viirret
-- SPDX-License-Identifier: LGPL-3.0-or-later
-- LuaJIT binding for the public nexilisc client ABI.

local ffi = require("ffi")

ffi.cdef[[
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef struct nexilis_ClientConfigC nexilis_ClientConfigC;
typedef struct nexilis_ClientAPI nexilis_ClientAPI;
typedef struct nexilis_ProtocolManagerC nexilis_ProtocolManagerC;
typedef struct nexilis_BoostTCPClient nexilis_BoostTCPClient;
typedef struct nexilis_BoostUDPClient nexilis_BoostUDPClient;
typedef struct nexilis_RoomsCollection nexilis_RoomsCollection;
typedef struct nexilis_Room nexilis_Room;
typedef struct nexilis_ClientSession nexilis_ClientSession;
typedef struct { void *data; } nx_data_c;
typedef struct { long tv_sec; long tv_nsec; } nexilis_timespec;
int clock_gettime(int clock_id, nexilis_timespec *tp);
int usleep(unsigned int usec);
void free(void *ptr);
void nexilis_log_start_console_debugging(void);
void nexilis_log_stop_logging(void);
void nexilis_set_boost_tcp_port(uint16_t);
void nexilis_set_boost_udp_port(uint16_t);

nexilis_ClientConfigC *nexilis_client_config_create(void);
void nexilis_client_config_destroy(nexilis_ClientConfigC *);
void nexilis_client_config_set_password(nexilis_ClientConfigC *, const char *);
void nexilis_client_config_set_mode(nexilis_ClientConfigC *, int);
void nexilis_client_config_set_message_encryption(nexilis_ClientConfigC *, int);
void nexilis_client_config_set_tls(nexilis_ClientConfigC *, int);
void nexilis_client_config_set_inet_udp(nexilis_ClientConfigC *, const char *);
void nexilis_client_config_set_inet_tcp(nexilis_ClientConfigC *, const char *);
void nexilis_client_config_set_boost_tcp_address(nexilis_ClientConfigC *, const char *);
void nexilis_client_config_set_boost_udp_address(nexilis_ClientConfigC *, const char *);
void nexilis_client_config_set_unix_dgram_server_path(nexilis_ClientConfigC *, const char *);
void nexilis_client_config_set_unix_stream_server_path(nexilis_ClientConfigC *, const char *);

nexilis_ProtocolManagerC *nexilis_protocol_manager_create(void);
void nexilis_protocol_manager_destroy(nexilis_ProtocolManagerC *);
nexilis_ClientAPI *nexilis_client_api_create(nexilis_ClientConfigC *);
void nexilis_client_api_destroy(nexilis_ClientAPI *);
bool nexilis_client_api_is_initialized(const nexilis_ClientAPI *);
bool nexilis_client_api_is_boost_tcp_ready(const nexilis_ClientAPI *);
bool nexilis_client_api_is_boost_udp_ready(const nexilis_ClientAPI *);
uint64_t nexilis_client_api_get_client_id(const nexilis_ClientAPI *);
const char *nexilis_client_api_get_client_username(const nexilis_ClientAPI *, uint64_t);
size_t nexilis_client_api_rooms_count(const nexilis_ClientAPI *);
size_t nexilis_client_api_client_room_id(const nexilis_ClientAPI *);
nexilis_RoomsCollection *nexilis_client_api_get_active_rooms(const nexilis_ClientAPI *);
nexilis_Room *nexilis_client_api_get_room(const nexilis_ClientAPI *, uint64_t);
nexilis_ClientSession *nexilis_client_api_get_client_from_room(const nexilis_ClientAPI *, uint64_t);
char *nexilis_client_api_drain_game_events(nexilis_ClientAPI *, size_t);
void nexilis_client_api_free_events(char *);

size_t nexilis_rooms_collection_rooms_count(const nexilis_RoomsCollection *);
nexilis_Room *nexilis_rooms_collection_room_get(nexilis_RoomsCollection *, size_t);
void nexilis_rooms_collection_free(nexilis_RoomsCollection *);
void nexilis_room_destroy(nexilis_Room *);
uint64_t nexilis_room_get_id(nexilis_Room *);
uint64_t nexilis_room_get_client_amount(nexilis_Room *);
const char *nexilis_room_get_name(const nexilis_Room *);
int nexilis_room_get_context(const nexilis_Room *);
uint32_t nexilis_room_get_max_size(const nexilis_Room *);
uint64_t nexilis_room_get_creator_id(const nexilis_Room *);
nexilis_ClientSession **nexilis_room_get_clients(const nexilis_Room *, size_t *);
void nexilis_room_free_client_array(nexilis_ClientSession **, size_t);
uint64_t nexilis_client_session_get_id(nexilis_ClientSession *);
bool nexilis_client_session_get_position_3D_values(nexilis_ClientSession *, float *, float *, float *);
void nexilis_client_session_release_borrowed(nexilis_ClientSession *);

nexilis_BoostTCPClient *nexilis_boost_tcp_client_create(nexilis_ProtocolManagerC *, nexilis_ClientAPI *);
void nexilis_boost_tcp_client_destroy(nexilis_BoostTCPClient *);
void nexilis_boost_tcp_client_stop(nexilis_BoostTCPClient *);
bool nexilis_boost_tcp_client_is_connected(nexilis_BoostTCPClient *);
void nexilis_boost_tcp_client_send_message(nexilis_BoostTCPClient *, const uint8_t *, size_t);
bool nexilis_start_client(nexilis_ClientAPI *, nexilis_BoostTCPClient *);
nexilis_BoostUDPClient *nexilis_boost_udp_client_create(nexilis_ProtocolManagerC *, nexilis_ClientAPI *);
void nexilis_boost_udp_client_destroy(nexilis_BoostUDPClient *);
void nexilis_boost_udp_client_start(nexilis_BoostUDPClient *);
void nexilis_boost_udp_client_stop(nexilis_BoostUDPClient *);
bool nexilis_boost_udp_client_is_connected(nexilis_BoostUDPClient *);
void nexilis_boost_udp_client_send_message(nexilis_BoostUDPClient *, const uint8_t *, size_t);

uint64_t nexilis_nx_data_get_size(const nx_data_c *);
const uint8_t *nexilis_nx_data_get_data(const nx_data_c *);
void nexilis_nx_data_destroy(nx_data_c *);
nx_data_c nexilis_packet_get_general_clientId(nexilis_ClientAPI *);
nx_data_c nexilis_packet_get_info_general(nexilis_ClientAPI *);
nx_data_c nexilis_packet_get_info_clients(nexilis_ClientAPI *);
nx_data_c nexilis_packet_get_info_rooms(nexilis_ClientAPI *);
nx_data_c nexilis_packet_room_management_join(nexilis_ClientAPI *, uint64_t);
nx_data_c nexilis_packet_room_management_leave(nexilis_ClientAPI *);
nx_data_c nexilis_packet_room_management_create(nexilis_ClientAPI *, const char *, int);
nx_data_c nexilis_packet_room_player3D_position_direct(nexilis_ClientAPI *, float, float, float);
nx_data_c nexilis_packet_room_player3D_dimension_direct(nexilis_ClientAPI *, float, float, float);
nx_data_c nexilis_packet_room_player3D_movement_direct(nexilis_ClientAPI *, float, float, float, float);
nx_data_c nexilis_packet_room_player3D_set_team(nexilis_ClientAPI *, const char *);
nx_data_c nexilis_packet_room_player3D_shoot(nexilis_ClientAPI *, uint64_t, float);
nx_data_c nexilis_packet_room_player3D_audio_event(nexilis_ClientAPI *, uint8_t, float, float, float);
nx_data_c nexilis_packet_room_communicate_broadcast(nexilis_ClientAPI *, const char *);
]]

local native = ffi.load(os.getenv("NEXILISC_LIBRARY") or "nexilisc")
local null = ffi.NULL
local function check_ptr(ptr, operation)
    if ptr == nil or ptr == null then error(operation .. " failed", 3) end
    return ptr
end

local function id(value)
    if type(value) == "string" then
        assert(value:match("^%d+$"), "ID must contain decimal digits")
        local digits = value:gsub("^0+", "")
        if digits == "" then digits = "0" end
        assert(#digits < 20 or (#digits == 20 and digits <= "18446744073709551615"),
            "ID exceeds uint64_t range")
        local parsed = ffi.new("uint64_t", 0)
        for index = 1, #digits do parsed = parsed * 10 + tonumber(digits:sub(index, index)) end
        return parsed
    end
    if type(value) == "number" then
        assert(value >= 0 and value <= 9007199254740991 and value % 1 == 0,
            "numeric ID must be an exact nonnegative integer; use a decimal string for larger IDs")
    end
    return ffi.new("uint64_t", value)
end

local function decimal(value)
    local result = tostring(value):gsub("ULL$", ""):gsub("LL$", "")
    return result
end

local function owned_string(ptr, release)
    if ptr == nil or ptr == null then return nil end
    local value = ffi.string(ptr)
    release(ffi.cast("void *", ptr))
    return value
end

local function now_ms()
    local ts = ffi.new("nexilis_timespec[1]")
    assert(ffi.C.clock_gettime(1, ts) == 0, "clock_gettime failed")
    return tonumber(ts[0].tv_sec) * 1000 + tonumber(ts[0].tv_nsec) / 1000000
end

local function wait_until(predicate, timeout_ms, interval_ms)
    local deadline = now_ms() + timeout_ms
    while not predicate() do
        if now_ms() >= deadline then return false end
        ffi.C.usleep(math.max(1, interval_ms) * 1000)
    end
    return true
end

local function packet_bytes(packet)
    local data = ffi.new("nx_data_c[1]", packet)
    local size = tonumber(native.nexilis_nx_data_get_size(data))
    local ptr = native.nexilis_nx_data_get_data(data)
    local bytes = size > 0 and ffi.string(check_ptr(ptr, "packet data"), size) or ""
    native.nexilis_nx_data_destroy(data)
    return bytes
end

local function room_snapshot(room)
    return {
        id = native.nexilis_room_get_id(room),
        name = owned_string(native.nexilis_room_get_name(room), ffi.C.free),
        context = native.nexilis_room_get_context(room),
        max_size = tonumber(native.nexilis_room_get_max_size(room)),
        client_count = tonumber(native.nexilis_room_get_client_amount(room)),
        creator_id = native.nexilis_room_get_creator_id(room),
    }
end

local function release_state(state)
    if state.udp then
        native.nexilis_boost_udp_client_stop(state.udp)
        native.nexilis_boost_udp_client_destroy(state.udp)
        state.udp = nil
    end
    if state.tcp then
        native.nexilis_boost_tcp_client_stop(state.tcp)
        native.nexilis_boost_tcp_client_destroy(state.tcp)
        state.tcp = nil
    end
    if state.api then native.nexilis_client_api_destroy(state.api); state.api = nil end
    if state.manager then native.nexilis_protocol_manager_destroy(state.manager); state.manager = nil end
    if state.config then native.nexilis_client_config_destroy(state.config); state.config = nil end
end

local Client = {}
Client.__index = Client

function Client.new(options)
    options = options or {}
    local state = {}
    local guard = ffi.gc(ffi.new("uint8_t[1]"), function() release_state(state) end)
    local self = setmetatable({ state = state, guard = guard, options = options,
        remote = {}, chat_since = 0, ready = false, closed = false }, Client)
    local ok, err = pcall(function()
        state.config = check_ptr(native.nexilis_client_config_create(), "client config creation")
        native.nexilis_client_config_set_password(state.config, options.password or "password")
        native.nexilis_client_config_set_mode(state.config, options.authentication_mode or 2)
        native.nexilis_client_config_set_tls(state.config, options.tls and 1 or 0)
        native.nexilis_client_config_set_message_encryption(state.config, options.message_encryption and 1 or 0)
        local address = options.server_address or "127.0.0.1"
        native.nexilis_client_config_set_boost_tcp_address(state.config, options.tcp_address or address)
        native.nexilis_client_config_set_boost_udp_address(state.config, options.udp_address or address)
        if options.inet_tcp_address then native.nexilis_client_config_set_inet_tcp(state.config, options.inet_tcp_address) end
        if options.inet_udp_address then native.nexilis_client_config_set_inet_udp(state.config, options.inet_udp_address) end
        if options.unix_stream_path then native.nexilis_client_config_set_unix_stream_server_path(state.config, options.unix_stream_path) end
        if options.unix_dgram_path then native.nexilis_client_config_set_unix_dgram_server_path(state.config, options.unix_dgram_path) end
        state.api = check_ptr(native.nexilis_client_api_create(state.config), "client API creation")
        state.manager = check_ptr(native.nexilis_protocol_manager_create(), "protocol manager creation")
        state.tcp = check_ptr(native.nexilis_boost_tcp_client_create(state.manager, state.api), "TCP client creation")
        state.udp = check_ptr(native.nexilis_boost_udp_client_create(state.manager, state.api), "UDP client creation")
    end)
    if not ok then self:close(); error(err, 2) end
    return self
end

function Client:assert_open()
    assert(not self.closed, "Nexilis client is closed")
end

function Client:connect()
    self:assert_open()
    if not native.nexilis_start_client(self.state.api, self.state.tcp) then
        return false, "TCP connection or initialization failed"
    end
    native.nexilis_boost_udp_client_start(self.state.udp)
    if not native.nexilis_boost_udp_client_is_connected(self.state.udp) then
        return false, "UDP connection failed"
    end
    local timeout = self.options.room_timeout_ms or 5000
    local interval = self.options.poll_interval_ms or 50
    if self.options.auto_join == false then return true end
    if not wait_until(function() return native.nexilis_client_api_rooms_count(self.state.api) > 0 end, timeout, interval) then
        return false, "no rooms appeared before timeout"
    end
    return self:join_any_room()
end

function Client:is_connected()
    return not self.closed and native.nexilis_boost_tcp_client_is_connected(self.state.tcp)
        and native.nexilis_boost_udp_client_is_connected(self.state.udp)
end

function Client:is_initialized()
    self:assert_open()
    return native.nexilis_client_api_is_initialized(self.state.api)
end

function Client:transport_ready(transport)
    self:assert_open()
    if transport == "tcp" then return native.nexilis_client_api_is_boost_tcp_ready(self.state.api) end
    if transport == "udp" then return native.nexilis_client_api_is_boost_udp_ready(self.state.api) end
    error("transport must be 'tcp' or 'udp'", 2)
end

function Client:client_id()
    self:assert_open()
    return native.nexilis_client_api_get_client_id(self.state.api)
end

function Client:room_id()
    self:assert_open()
    return native.nexilis_client_api_client_room_id(self.state.api)
end

function Client:is_in_room()
    return self:room_id() ~= 0
end

function Client:room_count()
    self:assert_open()
    return tonumber(native.nexilis_client_api_rooms_count(self.state.api))
end

function Client:username(client_id)
    self:assert_open()
    return owned_string(native.nexilis_client_api_get_client_username(self.state.api, id(client_id)), ffi.C.free)
end

function Client:rooms()
    self:assert_open()
    local collection = check_ptr(native.nexilis_client_api_get_active_rooms(self.state.api), "room listing")
    local result = {}
    local count = tonumber(native.nexilis_rooms_collection_rooms_count(collection))
    for index = 0, count - 1 do
        local room = native.nexilis_rooms_collection_room_get(collection, index)
        if room ~= null then result[#result + 1] = room_snapshot(room) end
    end
    native.nexilis_rooms_collection_free(collection)
    return result
end

function Client:room(room_id)
    self:assert_open()
    local room = native.nexilis_client_api_get_room(self.state.api, id(room_id))
    if room == null then return nil end
    local info = room_snapshot(room)
    native.nexilis_room_destroy(room)
    return info
end

function Client:room_clients(room_id)
    self:assert_open()
    local room = native.nexilis_client_api_get_room(self.state.api, id(room_id))
    if room == null then return nil end
    local count = ffi.new("size_t[1]")
    local array = native.nexilis_room_get_clients(room, count)
    local clients = {}
    for index = 0, tonumber(count[0]) - 1 do
        local session = array[index]
        local x, y, z = ffi.new("float[1]"), ffi.new("float[1]"), ffi.new("float[1]")
        local position = nil
        if native.nexilis_client_session_get_position_3D_values(session, x, y, z) then
            position = { x = tonumber(x[0]), y = tonumber(y[0]), z = tonumber(z[0]) }
        end
        clients[#clients + 1] = { id = native.nexilis_client_session_get_id(session), position = position }
    end
    native.nexilis_room_free_client_array(array, count[0])
    native.nexilis_room_destroy(room)
    return clients
end

function Client:remote_client(client_id)
    self:assert_open()
    local session = native.nexilis_client_api_get_client_from_room(self.state.api, id(client_id))
    if session == null then return nil end
    -- The wrapper is ours; the underlying session belongs to ClientAPI.
    local x, y, z = ffi.new("float[1]"), ffi.new("float[1]"), ffi.new("float[1]")
    local position = nil
    if native.nexilis_client_session_get_position_3D_values(session, x, y, z) then
        position = { x = tonumber(x[0]), y = tonumber(y[0]), z = tonumber(z[0]) }
    end
    local result = { id = native.nexilis_client_session_get_id(session), position = position }
    native.nexilis_client_session_release_borrowed(session)
    return result
end

function Client:send_packet(bytes, transport)
    self:assert_open()
    assert(self:is_connected(), "client is not connected")
    assert(type(bytes) == "string" and #bytes > 0, "packet must be a nonempty byte string")
    transport = transport or "tcp"
    if transport == "tcp" then
        native.nexilis_boost_tcp_client_send_message(self.state.tcp, bytes, #bytes)
    elseif transport == "udp" then
        native.nexilis_boost_udp_client_send_message(self.state.udp, bytes, #bytes)
    else
        error("transport must be 'tcp' or 'udp'", 2)
    end
    return true
end

function Client:packet(kind, ...)
    self:assert_open()
    local factory = Client.packet_factories[kind]
    assert(factory, "unknown packet kind: " .. tostring(kind))
    return packet_bytes(factory(self.state.api, ...))
end

Client.packet_factories = {
    client_id = native.nexilis_packet_get_general_clientId,
    info_general = native.nexilis_packet_get_info_general,
    info_clients = native.nexilis_packet_get_info_clients,
    info_rooms = native.nexilis_packet_get_info_rooms,
    join = function(api, room_id) return native.nexilis_packet_room_management_join(api, id(room_id)) end,
    leave = native.nexilis_packet_room_management_leave,
    create = function(api, name, context) return native.nexilis_packet_room_management_create(api, name, context or 1) end,
    position = native.nexilis_packet_room_player3D_position_direct,
    dimension = native.nexilis_packet_room_player3D_dimension_direct,
    movement = native.nexilis_packet_room_player3D_movement_direct,
    set_team = native.nexilis_packet_room_player3D_set_team,
    shoot = function(api, target, damage) return native.nexilis_packet_room_player3D_shoot(api, id(target), damage) end,
    audio_event = native.nexilis_packet_room_player3D_audio_event,
    broadcast = native.nexilis_packet_room_communicate_broadcast,
}

function Client:send(kind, transport, ...)
    return self:send_packet(self:packet(kind, ...), transport)
end

function Client:join_room(room_id)
    self:assert_open()
    room_id = id(room_id)
    assert(room_id ~= 0, "room ID must not be zero")
    if self.ready and self:room_id() == room_id then return true end
    self:send("join", "tcp", room_id)
    local joined = wait_until(function() return self:room_id() == room_id end,
        self.options.room_timeout_ms or 5000, self.options.poll_interval_ms or 50)
    if joined then
        self.ready = true
        self.remote = {}
        if self.options.on_ready then self.options.on_ready(self, self:room(room_id)) end
    end
    return joined
end

function Client:join_any_room()
    local rooms = self:rooms()
    if #rooms == 0 then return false, "no rooms available" end
    local chosen = nil
    if self.options.preferred_room_id then
        for _, room in ipairs(rooms) do
            if room.id == id(self.options.preferred_room_id) then chosen = room; break end
        end
    end
    if not chosen then
        for _, room in ipairs(rooms) do
            if room.client_count < room.max_size then chosen = room; break end
        end
    end
    return self:join_room((chosen or rooms[#rooms]).id)
end

function Client:leave_room()
    if self:room_id() == 0 then return false end
    self:send("leave", "tcp")
    local left = wait_until(function() return self:room_id() == 0 end,
        self.options.room_timeout_ms or 5000, self.options.poll_interval_ms or 50)
    if left then
        self.ready = false
        self.remote = {}
        if self.options.on_left_room then self.options.on_left_room(self) end
    end
    return left
end

function Client:create_room(name, context)
    assert(type(name) == "string" and name:match("%S"), "room name must not be empty")
    local before = {}
    for _, room in ipairs(self:rooms()) do before[decimal(room.id)] = true end
    self:send("create", "tcp", name, context or 1)
    local created
    local appeared = wait_until(function()
        local fallback
        for _, room in ipairs(self:rooms()) do
            if not before[decimal(room.id)] then
                if room.creator_id == self:client_id() then created = room; return true end
                fallback = fallback or room
            end
        end
        created = fallback
        return created ~= nil
    end, self.options.room_timeout_ms or 5000, self.options.poll_interval_ms or 50)
    if not appeared then return false, "new room did not appear before timeout" end
    return self:join_room(created.id)
end

function Client:send_position(x, y, z) return self:send("position", "udp", x, y, z) end
function Client:send_dimensions(x, y, z) return self:send("dimension", "tcp", x, y, z) end
function Client:send_movement(x, y, z, dt) return self:send("movement", "udp", x, y, z, dt) end
function Client:set_team(team) return self:send("set_team", "tcp", team) end
function Client:shoot(target_id, damage) return self:send("shoot", "tcp", target_id, damage) end
function Client:send_audio_event(sound, x, y, z) return self:send("audio_event", "udp", sound, x, y, z) end
function Client:broadcast(message) return self:send("broadcast", "tcp", message) end

function Client:drain_events_json(chat_since)
    self:assert_open()
    local ptr = native.nexilis_client_api_drain_game_events(self.state.api, chat_since or self.chat_since)
    local json = owned_string(ptr, native.nexilis_client_api_free_events)
    if json then
        local total = json:match('"chatTotal"%s*:%s*(%d+)')
        if total then self.chat_since = tonumber(total) end
    end
    return json
end

function Client:update()
    self:assert_open()
    if not self.ready then return end
    local clients = self:room_clients(self:room_id()) or {}
    local present = {}
    for _, client in ipairs(clients) do
        if client.id ~= 0 and client.id ~= self:client_id() then
            local key = decimal(client.id)
            present[key] = true
            if not self.remote[key] and self.options.on_remote_client_added then
                self.options.on_remote_client_added(self, client.id)
            end
            self.remote[key] = client
            if client.position and self.options.on_remote_client_position then
                self.options.on_remote_client_position(self, client.id, client.position)
            end
        end
    end
    for key, client in pairs(self.remote) do
        if not present[key] then
            self.remote[key] = nil
            if self.options.on_remote_client_removed then self.options.on_remote_client_removed(self, client.id) end
        end
    end
end

function Client:remote_client_ids()
    local result = {}
    for _, client in pairs(self.remote) do result[#result + 1] = client.id end
    return result
end

function Client:close()
    if self.closed then return end
    self.closed = true
    self.ready = false
    ffi.gc(self.guard, nil)
    release_state(self.state)
    self.remote = {}
    if self.options.on_disconnected then self.options.on_disconnected(self) end
end

return {
    Client = Client,
    new = Client.new,
    native = native,
    id = id,
    id_string = decimal,
    RoomContext = { TWO_D = 0, THREE_D = 1, UNDEFINED = 2 },
    AuthenticationMode = { EMPTY = 0, SKIP = 1, PASSWORD_PROTECTED = 2, ADMIN_ACCESS = 3, ROOT_ACCESS = 4 },
}
