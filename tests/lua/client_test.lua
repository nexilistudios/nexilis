-- Copyright (C) 2026 Valtteri Viirret
-- SPDX-License-Identifier: LGPL-3.0-or-later

package.path = "bindings/lua/?.lua;" .. package.path
local nexilis = require("nexilis")

nexilis.native.nexilis_set_boost_tcp_port(55400)
nexilis.native.nexilis_set_boost_udp_port(55401)

assert(nexilis.id_string(nexilis.id("18446744073709551615")) == "18446744073709551615")
local ok = pcall(nexilis.id, "18446744073709551616")
assert(not ok)

local ready_count, leave_count = 0, 0
local client = nexilis.new({
    password = "lua-test-password",
    auto_join = false,
    room_timeout_ms = 3000,
    on_ready = function() ready_count = ready_count + 1 end,
    on_left_room = function() leave_count = leave_count + 1 end,
})

assert(#client:packet("info_rooms") > 0)
assert(#client:rooms() == 0)
assert(client:room_count() == 0)

if arg[1] == "integration" then
    local connected, reason = client:connect()
    assert(connected, reason)
    assert(client:is_connected())
    assert(client:is_initialized())
    assert(client:transport_ready("tcp"))
    assert(client:transport_ready("udp"))
    assert(client:client_id() ~= 0)
    assert(client:create_room("Lua test room", nexilis.RoomContext.THREE_D))
    assert(client.ready)
    assert(client:room_id() ~= 0)
    assert(client:is_in_room())
    assert(client:room(client:room_id()).name == "Lua test room")
    assert(#client:room_clients(client:room_id()) >= 1)
    assert(client:remote_client(client:client_id()) ~= nil)
    assert(#client:packet("shoot", client:client_id(), 1) > 0)
    local room_id = client:room_id()
    assert(client:leave_room(), "initial leave failed; room ID is " .. nexilis.id_string(client:room_id()))
    assert(client:join_room(room_id))
    client:send_position(1, 2, 3)
    client:send_dimensions(1, 2, 3)
    client:send_movement(1, 0, 0, 0.1)
    client:set_team("red")
    client:broadcast("hello from Lua")
    client:send_audio_event(1, 1, 2, 3)
    client:update()
    assert(type(client:drain_events_json()) == "string")
    local left = client:leave_room()
    assert(left, "leave failed; room ID is " .. nexilis.id_string(client:room_id()))
    assert(not client:is_in_room())
    assert(ready_count == 2 and leave_count == 2)
end

client:close()
client:close()
print("lua client: ok")
