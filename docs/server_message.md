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

After a request is authenticated and accepted by the replay window, the server applies a per-client command throughput limit before parsing or dispatching it. The limiter is a token bucket with a default rate of 120 commands per second and a burst allowance of one second's worth; excess commands are dropped without a response. Configure it with `nexilis::server::ServerConfig::setMaxCommandRate`; zero or a negative value disables the limit.

Clients send the initial authentication payload after their transport socket is
set up. Boost TCP also completes its TCP and TLS-PSK connection before sending
the payload. The client's `waitUntil*Ready()` checks run after this exchange
because they wait for the server's authentication response.

The Boost TCP server rejects request payload lengths above `NEXILIS_BUFFER`
(1024 bytes, including request headers and MAC) before allocating a body and
closes the connection. Once frame data is readable, the complete length prefix
and body must arrive within five seconds; partial reads do not reset this
deadline. TLS handshakes have a separate five-second deadline. Idle polling
alone does not close established connections. These limits apply to incoming
server requests, not to the Boost TCP client's JSON response parser.

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
relay that yaw, and the legacy form uses zero. The legacy movement handler
rejects durations outside (0, 0.25] seconds and applies each accepted move
during command dispatch. The 2D movement request uses the same duration range
and applies its vector as one displacement rather than streaming tick updates.

An application room can disable client position requests and install a movement
input handler. In that room, the three movement floats are input direction and
jump intent, the client delta time is ignored, and the application updates and
broadcasts positions on its server tick. The application must validate speed,
collisions, and input age. Client position requests (`room/player_3D/position`)
are disabled by default; a room that wants client-authoritative positions must
explicitly call `setClientPositionAllowed(true)`. Other rooms retain the legacy
movement behavior.

The same room setting gates `room/player_2D/position`. This request requires
exactly two finite floats; the server clamps both 2D coordinates to the
configured maximum horizontal position before storing and broadcasting them.
Malformed or non-finite positions are rejected.

An application room must install a shot handler to accept shots.
`room/player_3D/shoot` requires three floats for aim direction followed by one
byte for weapon slot (1 primary, 2 secondary, 3 knife) and one action byte
(0 fire, 1 reload). Target ID and damage requests are rejected in every room.
The handler computes the hit and damage from server state; a request with no
valid hit does not damage anyone. Rooms without a handler reject shot requests.

Client team selection requests are rejected by default. An application must install
`Room::setTeamSelectionHandler` to allow them; the handler can reject a player based
on match state. The server accepts only the `Terrorist` and `Counter Terrorist`
team names from clients. An application can set teams directly on the server.

Client game item creation, status changes, and deletion are disabled by default.
A room can call `setClientGameItemCreationAllowed(true)` to allow them. Creation
requires finite positions within the configured world bounds, positive finite
dimensions no larger than the horizontal world bound, a type of 1–64 characters,
a status of 1–32 characters, and a relative asset path of 1–256 characters.
Types and statuses accept ASCII letters, digits, `_`, and `-`. Asset paths accept
those characters plus `.` and `/`, but reject absolute paths and `.` or `..`
segments. Client status updates in an opted-in room use the same status rules.

A room can separately install a pickup handler. Pickup requests use `room/game_item/update`
with an eight-byte item ID and the remaining string `picked_up`. The handler
must validate ownership, status, and proximity before the status is broadcast.
The nx-3D handler consumes the item on pickup and follows the status message
with a destroy message so all clients remove it.

Client room object creation, movement, and deletion are disabled by default.
A room can call `setClientObjectCreationAllowed(true)` to allow
`room/object_2D` and `room/object_3D` create, create_moving, move, and destroy
requests. Creation requires finite positions within the configured world bounds,
positive finite dimensions no larger than the horizontal world bound, and an
optional relative asset path of at most 256 characters. Asset paths reject
absolute paths and `.` or `..` segments. Moving object requests also require
finite movement values and a duration in (0, 0.25] seconds. Move offsets must be
finite; the resulting position is clamped to the configured world bounds. Rooms
that have not opted in reject these requests, and object handlers reject
commands for rooms that no longer exist.

Moving-object creation sends the create message followed by one final position
update during dispatch. Both eased and linear requests apply their full
requested displacement immediately, with the resulting position clamped to the
world bounds. The validated duration no longer schedules background ticks.
Applications needing intermediate animation must update objects on their own
server tick. The C++ `Movement::object2D` and `object3D` helpers now return
`void` and complete before returning; callers must remove thread operations
such as `.detach()` or `.join()`.

Client-requested room removal is disabled by default. A room that wants its
creator to be able to remove it through `room/management/remove` must call
`Room::setClientRoomRemovalAllowed(true)`; only the room creator can remove it,
and applications can still remove rooms directly on the server. Only the room
creator may change the room-wide overlap setting through
`room/management/set_overlap`; other members receive an unauthorized result.

Client-requested room creation is capped at 1024 rooms per server and 64 rooms
per client by default. The limits are configurable through
`nexilis::server::ServerConfig::getRoomLimits()` as `setMaxRooms` and
`setMaxRoomsPerClient`; requests beyond either limit are rejected. Room creation
remains available to authenticated clients, so applications that need stricter
authorization should enforce it before dispatching the command.

The server enforces gameplay limits on `room/player_3D/position`,
`room/player_3D/movement` and `room/player_3D/shoot`. Every limit has a
default and is configurable through `nexilis::server::ServerConfig::getGameplayLimits()`:

- Maximum horizontal position (absolute X and Z coordinate): 10000
- Maximum movement speed (units per second): 100
- Maximum shooting rate (shots per second): 10; a non-positive value disables the limit
- Maximum broadcast message length (bytes): 1024
- Maximum broadcast rate (messages per second): 5; a non-positive value disables the limit
- Maximum damage per shot: 35
- Minimum and maximum height (Y coordinate): 0 and 1000

Positions outside their limits are clamped to the nearest allowed value (the
clamped position is applied and broadcast), movement speeds above the maximum
are scaled down, damage above the per-shot cap is reduced to it, and shots
that arrive sooner than the shooting rate allows are dropped. The shooting
rate applies to fire requests before the room's shot handler runs, including
misses. Reload requests do not consume this cooldown.

`room/communication/broadcast` messages longer than the configured limit, or
arriving faster than the configured broadcast rate, are rejected as invalid
input before they are stored or relayed. The per-client rate limit applies to
the encrypted broadcast form as well, since both forms use the same handler.
Each room retains at most the 100 most recent broadcasts and replays only those
to joining clients; this history bound is currently fixed in
`nexilis_constants.hh`. Broadcasts still reach every member of the room,
including the sender, and the server does not moderate their contents.

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

## Room creation

Room creation requests use a 3-byte room-management command header, followed by a
1-byte room context, a 2-byte unsigned maximum player count in the configured
wire byte order, and a room name of 1–64 bytes (a single space is rejected).
The server checks the complete payload length before indexing or copying its
fields, rejecting truncated and oversized requests. It accepts capacities from 1 to 64,
includes `max_size` in the creation response and room listing, and rejects joins
when the room is full. Applications can install a `RoomStorage::setCreateHandler`
callback to configure each new room before the server acknowledges creation.

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

New registrations in both `skip` and `password_protected` modes are limited to
20 successful handshakes per source address per minute. A new `skip` user must
send the canonical 19-byte `getting/client_id` handshake. Unknown or malformed
requests do not allocate a user. The shared client registry admits at most
`NEXILIS_DEFAULT_MAX_CLIENTS` live users (currently 1024); further handshakes
are rejected until users disconnect or expire.

`ClientAPI::setMessageEncryption(true)` affects room broadcasts when the client has a nonempty password. It does not encrypt othercast or unicast messages. The message key is derived from that same password and the room ID; the server holds the password, so this feature does **not** hide message contents from the server.

The current broadcast implementation also falls back to the plaintext payload if key derivation or encryption fails. Treat this option as experimental and do not rely on it for confidentiality until that behavior is changed and tested. TLS-PSK protects only the Boost TCP transport when both peers leave it enabled and use the same passphrase.

## Server room storage ownership

The C++ `server::RoomStorage::getRoomById` returns `std::shared_ptr<Room>`.
Keep that handle alive while accessing the room. `getAllRooms()` returns a
copied vector of shared handles; it no longer exposes the registry itself.
Use `RoomStorage::remove(id)` to unpublish a room. Existing handles remain valid
after removal or `clear()`, so operations already holding a handle may finish.
Callers using `auto*` for lookups must change to `auto`; callers must not keep a
raw pointer obtained from a temporary handle.

`RoomStorage::add` returns `bool`, rejects duplicate IDs, and accepts optional
server/per-creator caps checked atomically with insertion. Client-requested
creation passes the configured caps. Create callbacks run after publication,
outside the registry mutex, and may reenter storage. The callback's room remains
alive until the callback returns, even if the callback removes it.

The registry mutex protects lookup, insertion, enumeration, removal, and callback
registration. `Room::getClients()` returns a member-ID snapshot, and
`tryJoinRoom()` checks capacity and inserts under one lock. Server command paths
take locked snapshots of room objects and items. Applications that access the
inherited `BaseRoom` object vectors directly must synchronize that access with
their own concurrent mutations.

## Server client storage ownership

The C++ `server::ClientStorage::getClientById` returns `std::shared_ptr<User>`.
Keep the handle alive throughout an operation. `getAllClients()` and
`getClientsByIpAddress()` return copied vectors of owning handles, safe to
iterate while the registry changes. Removal and `clear()` unpublish users;
existing handles and incoming messages keep their users alive until dispatch
finishes. `BaseMessage::Data` now takes a shared user handle; `getUser()` borrows
from that message and must not outlive it.

`ClientStorage::add` returns `bool` and accepts a shared handle (a moved
`unique_ptr` is converted to shared ownership). Null users and duplicate IDs are
rejected. The root ID-change command uses `ClientStorage::updateId` to change a
registered ID under the mutex and reject collisions. Use `auto` rather than
`auto*` for lookups, and keep the returned handle instead of extracting a raw
pointer from a temporary.

The registry mutex and shared handles protect container access and lifetime.
User send callbacks and username access are locked; user ID and room ID are
atomic, and object fields have locks. These locks do not turn several field
reads into one coherent game-state snapshot or revoke a command already in
progress when its user is removed.

Room sends tolerate stale membership IDs: a missing client is skipped and
remaining recipients are still attempted. `Command::sendRoomCommand` returns
`false` if any recipient is missing or cannot be sent the message; a false
result does not mean that no recipients received it.

## User and transport lifecycle

A user is published only after its handshake succeeds. It remains registered
while at least one stream connection, datagram transport, or pending Boost TCP
handshake is active. Ending one transport clears only that transport's send
callback when another transport keeps the user alive. When the last transport
ends, the server broadcasts the room leave command, removes the user from the
room and `ClientStorage`, and leaves retained callbacks intact so they continue
to reject sends through the closed connection.

`ServerConfig::setDatagramIdleTimeout` controls how long a datagram transport
may stay idle before its presence expires. The default is 120 seconds; a
non-positive value disables idle expiry. Each valid datagram refreshes the
activity timestamp. A stream connection or pending handshake keeps the same
user registered after its datagram transport expires.

Boost TCP tracks the user published by a successful handshake until the session
connection identifies it. A handshake that closes without advertising a session
cleans up immediately; one that advertises a session but never opens it expires
after 30 seconds or when the protocol stops. Datagram transports similarly drop
their remaining local users when the protocol stops.

The raw IPv4 TCP server accepts at most 128 simultaneous connections. It
reclaims completed connection workers, and each frame must arrive within ten
seconds from the start of its read. The Unix stream server applies the same complete
frame deadline to its one local connection. Boost TCP sends and receives have
five-second operation deadlines; shutdown joins their workers before destroying
the server.

## Command dispatch errors

Both C++ `Command::read` overloads contain parsing and callback exceptions and
return `CommandResult::error`. All server transports use this boundary.
Boost UDP also contains exceptions in the surrounding datagram-processing loop,
so a failed request does not stop later datagrams from being processed.
A dispatch error does not roll back changes made before the exception: for
example, creation can succeed before an acknowledgement callback throws.

## Linux Unix socket restrictions

Unix stream and datagram paths must be nonempty filesystem paths without NULs,
shorter than `sockaddr_un::sun_path` (108 bytes on Linux, including the required
terminator). Datagram clients also validate their longer generated reply path.
Invalid paths and socket bind/listen failures stop setup. Existing files and
socket nodes are never removed to make room for a new binding; clean up stale
nodes explicitly. Created socket files use permissions `0600`.

Stream peers are checked with `SO_PEERCRED`; both datagram endpoints verify
`SCM_CREDENTIALS` supplied by the kernel using `SO_PASSCRED`. The peer UID must
match the process's effective UID. Missing/truncated credentials are rejected.
This restricts access by local account, while processes sharing that account
still need application authentication. Place socket paths in a trusted directory.

Unix stream server shutdown wakes blocked accepts/reads before joining workers.
Its stored send callbacks retain connection state and reject sends after closure,
including after the server is destroyed. Unix stream servers and datagram
protocols must be stopped before moving them while a worker is joinable.

Unix datagram server callbacks also retain socket state independently of the
server object. Shutdown disables sends, joins the receive worker, and closes the
descriptor while holding the send mutex. Retained callbacks throw
`std::runtime_error` after shutdown, including after destruction or replacement
of the server. Sends are nonblocking; socket errors (including a full peer queue)
throw `std::system_error`. A callback does not keep a stopped server's socket open.

## IPv4 TCP server callback lifetime

Raw `af_inet` TCP send callbacks retain shared connection state independently
of the server. A connection mutex serializes complete length-prefixed response
frames with shutdown and close. Sends are nonblocking; a send failure, including
a full socket send buffer, shuts down the connection and throws
`std::system_error` rather than leaving a partial frame usable.

Disconnect closes the connection and disables retained callbacks. Server stop
disables sends and wakes blocked reads, then joins workers before returning.
Retained callbacks throw `std::runtime_error` after disconnect, stop, destruction,
or move assignment replacing their server; they cannot target a recycled socket
descriptor. `TCPServer::Client` now shares ownership of its connection, and
`getSocket()` returns a borrowed descriptor, or `-1` after closure.

Moving a server with any joinable worker throws `std::logic_error`. Move
assignment stops the destination first. Repeated starts are ignored, and stopped
instances cannot restart. Call lifecycle methods from one controlling thread.
A dispatch callback can request stop, but an external `stop()` must join the
workers before destruction or moving. Transport closure now removes the user
and its room membership when nothing else keeps the user alive, and user send
callbacks are synchronized on the user object.

## IPv4 UDP server callback lifetime

Raw `af_inet` UDP server callbacks retain socket state independently of the
server object. Shutdown disables sends, joins the receive worker, and closes the
descriptor under the send mutex. Retained callbacks throw `std::runtime_error`
after stop, destruction, or move assignment replacing their server. Sends are
nonblocking and socket errors throw `std::system_error`; callback ownership does
not keep a stopped socket open. Datagram-processing exceptions are contained by
the receive worker so send failures during shutdown cannot terminate the process.
Socket creation, receive-timeout setup, and bind failures throw during construction.

An IPv4 UDP server with a joinable receive worker must be stopped before moving
it; attempting to move a running source throws `std::logic_error`. Move assignment
stops the destination before replacing its socket. Repeated `start()` calls are
ignored while its worker is joinable, and a stopped server cannot restart.
These changes do not synchronize mutable user fields or callback installation.

## Boost UDP server callback lifetime

Boost UDP send callbacks retain shared socket state independently of the server.
All socket operations use the same mutex; receives and sends are nonblocking.
Shutdown disables sends, joins the receive worker, then closes the socket under
that mutex. Retained callbacks throw `std::runtime_error` after shutdown,
destruction, or move assignment replacing their server. Socket errors throw
`boost::system::system_error`. The shared state also owns the Asio context, so
retained callbacks cannot outlive the context their socket requires.

Moving a source with a joinable receive worker throws `std::logic_error` before
changing it. Move assignment stops the destination first. Repeated `start()`
calls are ignored, and stopped instances cannot restart. Call lifecycle methods
from one controlling thread. A receive-thread callback can request stop; an
external call to `stop()` must then join the worker before destruction or moving.
User send callbacks are synchronized on the user object, and idle datagram users
are removed after the configured timeout.

## Random identifiers

The C++ `Util` random helpers use OpenSSL `RAND_bytes` for identifiers, integer
ranges, and uppercase random strings. IDs remain nonzero. Random-source failures
throw `std::runtime_error` rather than falling back to a predictable generator.
`getRandomInt` rejects reversed ranges with `std::invalid_argument`; bounds are
inclusive. Identifiers remain public values and do not replace authentication
or storage collision checks.

The C `nexilis_room_data_create` returns null on allocation or random-source
failure, and the C# `RoomData` constructor reports a construction exception.

## Client-created resource limits

`ServerConfig::getRoomLimits()` also controls client-created resources:

- `setMaxObjectsPerRoom`: 1024 combined 2D/3D objects by default, including moving objects.
- `setMaxGameItemsPerRoom`: 1024 game items per room by default.
- `setMaxUsernameLength`: 64 username bytes by default.

Zero disables the corresponding client operation. Object/item requests at the
cap fail before insertion or broadcast; deletions free capacity. Username
updates above the configured size are rejected before state changes or sending.
The room opt-in gates remain required for client object/item creation.

Server `Room::tryAddObject` and `tryAddGameItem` check capacity and insert under
a resource mutex shared with server additions and deletions. Direct server
`addObject`/`addGameItem` calls intentionally bypass the client caps; their
resources still count against subsequent client creation. These methods do
not synchronize outstanding pointers/references returned by room accessors.
Applications must synchronize concurrent access to mutable room contents.

## Numeric gameplay validation

Player dimension requests require exactly two floats in 2D or three in 3D.
Every dimension must be finite, positive, and at most the configured maximum
horizontal position. Rejected requests neither change dimensions nor broadcast.

2D visual shot requests require exactly two finite direction floats; zero
vectors are rejected. The server normalizes accepted directions using a wider
length calculation to handle extreme finite float values safely. Audio-event
payloads require exactly one sound byte and three finite position floats within
the configured world bounds; trailing bytes are rejected.

These checks bound numerical inputs. Applications still decide whether clients
may select collider dimensions and enforce authoritative collision/game rules.
