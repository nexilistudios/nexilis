# Lua client binding

`nexilis.lua` is a LuaJIT FFI client for the `nexilisc` shared library. It covers
the client flows exposed by the C API: Boost TCP and UDP connection, room
listing and management, remote player snapshots, packet creation and sending,
position and movement, team, shooting, audio, chat, and game event draining.
The module is currently for 64 bit Linux. It needs LuaJIT and the matching
`libnexilisc.so` and `libnexilis.so` from this checkout.

Build the native libraries first (`cmake -B nexilis/build nexilis && cmake
--build nexilis/build`). Initialize the Boost module submodules for a local CMake build,
or configure with a system Boost.JSON installation in CI mode. Set
`NEXILISC_LIBRARY` to the absolute path of `libnexilisc.so`; CMake places both
shared libraries in `nexilis/build`.

```sh
export NEXILISC_LIBRARY="$PWD/nexilis/build/libnexilisc.so"
export LUA_PATH="$PWD/bindings/lua/?.lua;;"
luajit my_client.lua
```

```lua
local nexilis = require("nexilis")

local client = nexilis.new({
    server_address = "127.0.0.1",
    password = "password",
    authentication_mode = nexilis.AuthenticationMode.PASSWORD_PROTECTED,
    auto_join = false,
    on_ready = function(_, room) print("joined", room.name) end,
    on_remote_client_position = function(_, id, position)
        print(nexilis.id_string(id), position.x, position.y, position.z)
    end,
})

local connected, reason = client:connect()
if not connected then
    client:close()
    error(reason)
end

for _, room in ipairs(client:rooms()) do
    print(nexilis.id_string(room.id), room.name,
          room.client_count .. "/" .. room.max_size)
end

assert(client:join_any_room()) -- or client:join_room(id)
client:send_position(1, 2, 3)
client:broadcast("hello")
client:update() -- call each game tick for remote player callbacks
print(client:drain_events_json())
client:close()
```

`client:create_room(name, context)` creates and joins a room. Use
`nexilis.RoomContext.TWO_D` or `THREE_D`. `client:leave_room()` waits for the
server to confirm a leave. `client:room(id)`, `client:rooms()`,
`client:room_clients(id)`, and `client:remote_client(id)` return Lua snapshots.
`client:room_count()`, `client:is_in_room()`, `client:is_connected()`,
`client:is_initialized()`, and `client:transport_ready("tcp" | "udp")`
report connection state. Connection and room methods block for their configured
timeout; call them from a coroutine or worker if the host needs a responsive UI.
The `on_remote_client_added`, `on_remote_client_removed`, `on_remote_client_position`,
`on_left_room`, and `on_disconnected` callbacks are optional constructor options.

`client:packet(kind, ...)` returns a binary Lua string. Supported kinds are
`client_id`, `info_general`, `info_clients`, `info_rooms`, `join`, `leave`,
`create`, `position`, `dimension`, `movement`, `set_team`, `fire`, `audio_event`, and
`broadcast`. Use `client:send_packet(bytes, "tcp" | "udp")` to send custom
packets. Convenience methods route reliable packets over TCP and position,
movement, and audio packets over UDP.

`client:fire(x, y, z, slot)` sends an aim direction and weapon slot for the
room's server-side shot handler. The old `shoot` packet with a client-selected
target and damage is rejected by servers.

IDs are LuaJIT `uint64_t` values so the full 64 bit range survives. Pass an ID
as that value, an exact Lua number up to 2^53 - 1, or a decimal string. Use
`nexilis.id_string(id)` to print or serialize one. `drain_events_json()`
returns the native event JSON for damage, respawns, leaderboard, chat, and
audio. It tracks the chat cursor between calls; pass an explicit cursor to
override it.

`client:close()` stops transports and releases native resources. It is safe to
call more than once, and a GC finalizer is a backup. Do not retain raw native
pointers after closing the client. `nexilis.native` exposes the declared C
functions for advanced uses, but resource ownership remains the caller's
responsibility.

Boost TCP uses TLS-PSK by default. Set `password` to the server's nonempty
passphrase; set `tls = false` only when connecting to a server that explicitly
disables TLS-PSK. UDP traffic does not use TLS-PSK.

Run the smoke and live server tests with `make test-lua`. This target is part of
`make test`; `make unit-test` remains C++, C, and C# only.
