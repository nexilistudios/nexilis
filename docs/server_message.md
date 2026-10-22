# Server Message Protocol

A client request to the Nexilis server consists of several parts. This page describes the current binary request format and the Boost TCP JSON response format; it is not a versioned compatibility specification.

## Framing and request authentication

Each TCP and Unix stream request is prefixed with a **4-byte big-endian uint32_t length prefix** containing the size of the remaining payload. This preserves request boundaries over the stream.

```
[4 bytes: payload length (big-endian uint32_t)]
[8 bytes: client ID]
[8 bytes: message ID]
[16 bytes: HMAC-SHA-256 tag]
[remaining: command bytes + parameters]
```

The receiver reads exactly 4 bytes for the length, then exactly that many bytes for the payload. The tag authenticates the client ID, message ID, and complete command using the random per-session `udp_token` key delivered in the initial `client_id` response. The key is never included in later requests. Initial password or `skip` authentication has no session key yet and is the only unsigned request. The server rejects unsigned or incorrectly signed commands before dispatch.

UDP messages do not use a length prefix — each datagram is a single message.
The Boost UDP and af_inet UDP transports add an authentication envelope. A
new client sends a 20-byte `NXUH` hello and receives a 20-byte `NXUC` cookie
challenge. It then sends `NXUA`, the 16-byte cookie, and its normal initial
authentication payload. The cookie is bound to the source IP and port and
expires after roughly one to two minutes. The server sends no application
response until the cookie has been returned. A valid authentication response
contains a random 16-byte `udp_token` alongside `client_id`.

Later requests use `NXUS`, the 8-byte client ID, message ID, a 16-byte MAC tag,
and command bytes. The server checks the MAC and source
IP/port before dispatching a command. A client authenticated over another
transport obtains its token in the `client_id` response and can use it for UDP.
The 1024-byte receive buffer leaves up to 1004 bytes for the normal request
payload. Legacy UDP datagrams without this envelope are dropped. UDP remains
plaintext: an on-path observer of the initial UDP authentication can capture
the session key. Use Boost TCP with TLS-PSK for security-sensitive traffic.

## Identification (uint64_t)

The first 8 bytes of the payload identify the client. A value of 0 indicates an unauthenticated client.

## Message ID (uint64_t)

The unique identifier for the message (8 bytes), used for correlating responses.
Clients issue IDs in increasing order, starting at 1. The server tracks a
1024-ID window per session across all transports. It accepts a valid signed
request once, including limited out-of-order delivery, and rejects duplicates
and IDs outside the window. The initial authentication exchange is exempt.

## Command bytes (std::vector<uint8_t>)

For signed requests, the command bytes come after the client ID, message ID, and MAC tag. The command hierarchy varies by operation:

- First byte: main command type (`nexilis/include/nexilis/command_type.hh`)
- Second byte: sub-command type; both the first and second bytes are required
- Third and fourth bytes: additional sub-command types when required by the command

The server rejects a command shorter than two bytes as invalid input. Missing optional command levels do not match an action.

## Command parameters

Parameters passed to `Packet` vary in size by command. Strings are written as raw bytes with **no length prefix or null terminator** and occupy the remaining bytes after fixed-size fields.

### Application match extension

The `room/player_3D/position` request carries three floats (`x`, `y`, `z`) and
may append one float yaw in radians. The server accepts both 12-byte legacy
positions and 16-byte positions with yaw, then relays `yaw` in its JSON position
message. Legacy packets use yaw zero. Yaw is measured about the vertical Y axis;
zero faces world -Z. Yaw is normalized into `[-pi, pi]` before it is stored or
broadcast. The C, C#, and Lua position helpers keep sending the legacy
12-byte form. When a client joins a room, the initial position messages for
existing players include their current yaw.

The `room/player_3D/movement` request similarly accepts its legacy three-float
velocity plus float delta time, or one additional float yaw. Movement responses
relay that yaw, and the legacy form uses zero.

An application room can disable client position requests and install a movement
input handler. In that room, the three movement floats are input direction and
jump intent, the client delta time is ignored, and the application updates and
broadcasts positions on its server tick. The application must validate speed,
collisions, and input age. Client position requests (`room/player_3D/position`)
are disabled by default; a room that wants client-authoritative positions must
explicitly call `setClientPositionAllowed(true)`. Other rooms retain the legacy
movement behavior.

An application room must install a shot handler to accept shots.
`room/player_3D/shoot` requires three floats for aim direction followed by one
byte for weapon slot (1 primary, 2 secondary, 3 knife) and one action byte
(0 fire, 1 reload). Target ID and damage requests are rejected in every room.
The handler computes the hit and damage from server state; a request with no
valid hit does not damage anyone. Rooms without a handler reject shot requests.

An application room can also disable client game item creation and deletion
and install a pickup handler. Pickup requests use `room/game_item/update`
with an eight-byte item ID and the remaining string `picked_up`. The handler
must validate ownership, status, and proximity before the status is broadcast.
The nx-3D handler consumes the item on pickup and follows the status message
with a destroy message so all clients remove it.

The server enforces gameplay limits on `room/player_3D/position`,
`room/player_3D/movement` and `room/player_3D/shoot`. Every limit has a
default and is configurable through `nexilis::server::ServerConfig::getGameplayLimits()`:

- Maximum horizontal position (absolute X and Z coordinate): 10000
- Maximum movement speed (units per second): 100
- Maximum shooting rate (shots per second): 10; a non-positive value disables the limit
- Maximum damage per shot: 35
- Minimum and maximum height (Y coordinate): 0 and 1000

Positions outside their limits are clamped to the nearest allowed value (the
clamped position is applied and broadcast), movement speeds above the maximum
are scaled down, damage above the per-shot cap is reduced to it, and shots
that arrive sooner than the shooting rate allows are dropped. The shooting
rate applies to fire requests before the room's shot handler runs, including
misses. Reload requests do not consume this cooldown.

`Packet::Room::Player3D::matchAction(api, action)` sends room/player_3D action byte `8` followed by exactly one byte: `0` cancels, `1` begins planting, and `2` begins defusing. The room's application-installed action handler validates the request; rooms without a handler reject it. The server's combat callback can also deny hits by participants who are outside an active round. These callbacks are optional and do not change normal rooms.

An application can send a server-authored JSON room response with `type: "match"` and `action: "state"`. Nexilis exposes the parsed events through `consumeMatchEvents()` as owning pointers to the polymorphic `ClientAPI::MatchEvent` base. The `game_mode` field selects a derived event: `"bomb"` creates `BombMatchEvent`, while `"deathmatch"` creates `DeathmatchMatchEvent`. The base holds `room_id` and a virtual `kind()` discriminator. A missing `game_mode` is treated as bomb for compatibility with older messages; unknown modes are rejected.

Bomb snapshots include `round`, `terrorist_score`, `counter_terrorist_score`, `queue_position`, `phase`, `team`, `notice`, `seconds`, `bomb_planted`, `bomb_x/y/z`, `active`, `alive`, and `alive_players` (currently living player IDs). They are sent on state changes and about once per second. Deathmatch state messages include `player_id`, `alive`, and `respawn_seconds`; they are sent on death and respawn. This extension currently uses the Boost TCP sender used by nx-3D; other transports need an application sender before using these responses.

An application server can send a `room/player_3D` JSON response with
`action: "animation"`, `client_id`, `state`, `weapon`, and `duration`. Nexilis queues these
as `ClientAPI::AnimationEvent` values for `consumeAnimationEvents()`. The server
chooses the state; clients only use it to display the corresponding pose.
The application can use existing `match_action` requests for visible slot changes;
nx-3D uses action bytes 40 through 44 for slots one through five and validates
the equipped weapon against its server inventory before broadcasting it.

## Response format

`Get::Info::clients` replies as `info_general` with `client_amount`, the
server-wide connected client count. C++ clients expose the latest value through
`ClientAPI::getServerClientCount()`. Joining a room also sends a full leaderboard
seed so team selection screens can show the existing team roster.

The Boost TCP server sends JSON responses terminated by a newline character (`\n`); its client reads until `\n`. The POSIX `af_inet` TCP transport instead prefixes server responses with a 4-byte length. These transports should not be assumed to have interchangeable response framing.

## TLS-PSK transport protection

The Boost TCP transport uses TLS-PSK by default on both client and server. It encrypts TCP connections, including authentication. The pre-shared key is derived from the authentication passphrase; certificates are not used. Other transports do not implement TLS-PSK.

For a password protected connection, set the same nonempty passphrase on both sides. To use plaintext Boost TCP, both sides must explicitly opt out:

- `nexilis::server::ServerConfig::setTls(false)` on the server.
- `nexilis::client::ClientConfig::setTls(false)` (or `nexilis_client_config_set_tls` / the C# `ClientConfig.SetTls` / Lua `tls = false`) on every client.

Configuration details:

- The PSK is derived with PBKDF2-HMAC-SHA256 (100000 iterations) from the passphrase with the domain-separated salt `"nexilis-tls-psk-v1:"`, producing a 32-byte key (`nexilis::tls::derivePsk`).
- The code configures TLS 1.2 PSK cipher suites (`PSK-AES128-GCM-SHA256` and `PSK-CHACHA20-POLY1305`) and TLS 1.3 cipher suites (`TLS_AES_128_GCM_SHA256` and `TLS_CHACHA20_POLY1305_SHA256`). Verify the negotiated protocol version in each supported OpenSSL environment before making compatibility claims.
- A passphrase must be set on both server and client or the handshake fails. If the server has TLS enabled but cannot build a context (no passphrase), it refuses incoming connections. The client also cannot establish TLS without a password.
- If only one side enables TLS, the handshake fails rather than silently downgrading to plaintext.
- The handshake happens lazily: the server accepts it before the first message is read or written, and the client performs it right after the TCP connect. TLS is applied to both the initial connection and the fixed "switched" port the server uses for the authenticated session.

## Security notes

Command-channel passphrase authentication (`authentication`, action `2`) is
rejected; clients authenticate during the initial connection handshake. Root
authentication (`authentication`, action `1`) requires an established
`password_protected` session with common access over TLS-PSK Boost TCP and a
configured root password. Five failed root attempts from one source address
block further attempts from that address for ten minutes, including after a
reconnect. The attempt state is scoped to the server protocol instance and
resets when that instance is replaced. At most 4096 source addresses are tracked;
new addresses are denied root attempts while that limit is full.

The initial connection handshake for `password_protected` mode is separately
rate-limited: five failed passphrase attempts from one source address block
further handshake attempts from that address for ten minutes, including after a
reconnect. At most 4096 source addresses are tracked; while that limit is full,
new addresses are denied handshake attempts. A successful authentication clears
the recorded failures for the address. Like the root limiter, this state is
scoped to the server protocol instance and resets when that instance is
replaced.

`ClientAPI::setMessageEncryption(true)` affects room broadcasts when the client has a nonempty password. It does not encrypt othercast or unicast messages. The message key is derived from that same password and the room ID; the server holds the password, so this feature does **not** hide message contents from the server.

The current broadcast implementation also falls back to the plaintext payload if key derivation or encryption fails. Treat this option as experimental and do not rely on it for confidentiality until that behavior is changed and tested. TLS-PSK protects only the Boost TCP transport when both peers leave it enabled and use the same passphrase.
