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

Password mode can require a stable account identity. Configure
`ServerConfig::setAccountAuthenticator` with an application callback that
validates an account ID and private credential against a persistent account
store. Once configured, the server rejects legacy password-only handshakes.
The callback may run concurrently for separate connections and must be thread-safe.
Set both fields on `ClientConfig::setAccountCredentials`. The initial TLS
frame then contains eight zero bytes, `NXAC`, one-byte lengths for the shared
password, account ID, and credential, followed by those three byte strings in
that order. Each field is at most 255 bytes; the account ID and credential must
be nonempty. The server calls the account authenticator only after the shared
password succeeds. It publishes the validated account ID as `User::getAccountId`
and limits concurrent sessions for that ID to one by default; configure this
with `ServerConfig::setMaxSessionsPerAccount`. Applications should use the
stable account ID for bans, permissions, and account quotas, and can reject
banned or revoked credentials in the authenticator. The numeric client ID and
session tokens remain ephemeral. Password-only mode continues to work when no
authenticator is configured and does not identify a player account.

The Boost TCP server also reads requests already buffered by OpenSSL, including
an unprocessed TLS record after a previous request, without waiting for new
socket activity.

The Boost TCP server sends a separate random 16-byte `switch_token` (32 hex
characters) in the initial `port` response on the handshake connection. After
connecting to the switched port, the client sends one framed switch proof:
`[8-byte client ID][NXSW][32 ASCII hex token]`. The server accepts this frame
only while the handshake is pending, from the handshake's source IP, and
consumes the credential atomically. It rejects normal commands until this
proof succeeds. A session token alone cannot claim the switched socket; a
second socket cannot replace an active session. Later requests on the accepted
socket must retain the same client ID. In plaintext skip mode, a network
observer can see the switch credential; TLS protects it in password mode.
The switch proof changes the Boost TCP wire protocol, so the client and server
must both use versions that implement it.

After a request is authenticated and accepted by the replay window, the server applies a per-client command throughput limit before parsing or dispatching it. The limiter is a token bucket with a default rate of 120 commands per second and a burst allowance of one second's worth; excess commands are dropped without a response. Configure it with `nexilis::server::ServerConfig::setMaxCommandRate`; zero or a negative value disables the limit.

Clients send the initial authentication payload after their transport socket is
set up. Boost TCP also completes its TCP and certificate-based TLS connection before sending
the payload. The client's `waitUntil*Ready()` checks run after this exchange
because they wait for the server's authentication response.

The Boost TCP server rejects request payload lengths above `NEXILIS_BUFFER`
(1024 bytes, including request headers and MAC) before allocating a body and
closes the connection. Once frame data is readable, the complete length prefix
and body must arrive within five seconds; partial reads do not reset this
deadline. TLS handshakes have a separate five-second deadline. Idle polling
alone does not close established connections. A TLS read that completes at the
poll deadline retains its bytes so the next request frame stays aligned. These
limits apply to incoming server requests. Client JSON responses are limited to
16 MiB per message (excluding the newline delimiter); the Boost TCP receive
buffer enforces this limit before parsing, and the parser caps nesting at 32
levels.

Boost TCP accepts at most 64 workers waiting for initial authentication or a
session switch proof, and at most 512 connection workers overall by default.
Configure these limits before starting the server with
`ServerConfig::setMaxPendingBoostTcpConnections` and
`ServerConfig::setMaxBoostTcpConnections`; zero refuses new connections in the
corresponding category. Connections above a limit are closed. Completed worker
threads are joined and removed during normal accept-loop operation, including
when no new clients arrive. A connection has 10 seconds to complete its initial
authentication or session switch proof; established sessions have no such deadline.

UDP messages do not use a length prefix — each datagram is a single message.
The Boost UDP and af_inet UDP transports add an authentication envelope. A
new client sends a 20-byte `NXUH` hello and receives a 20-byte `NXUC` cookie
challenge. It then sends `NXUA`, the 16-byte cookie, and its normal initial
authentication payload. The cookie is bound to the source IP and port and
expires after roughly one to two minutes. The server sends no application
response until the cookie has been returned. A valid `skip`-mode authentication
response contains a random 16-byte `udp_token` alongside `client_id`. In
`password_protected` mode, initial UDP authentication is rejected. A session
established over pinned TLS Boost TCP can subsequently use Boost UDP with its
session token. Boost UDP traffic is authenticated but remains plaintext.

Later requests use `NXUS`, the 8-byte client ID, message ID, a 16-byte MAC tag,
and command bytes. The server checks the MAC and source
IP/port before dispatching a command. A skip-mode client obtains its token in
the `client_id` response and can use it for UDP.
The 1024-byte receive buffer leaves up to 1004 bytes for the normal request
payload. Legacy UDP datagrams without this envelope are dropped. UDP remains
plaintext, so it is unavailable in password-protected mode.

After setup, UDP server responses carry `NXUR`, an 8-byte response sequence,
a 16-byte HMAC-SHA-256 tag, and the JSON payload. The MAC uses the per-session
token with a response-specific domain label. Clients verify the tag and a
1024-response replay window before parsing JSON. The initial `skip`-mode
`client_id` reply is the only unsigned UDP application response because the
client does not know its token yet.

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
relay that yaw, and the legacy form uses zero. Rooms reject legacy 2D and 3D
movement by default. A server application may explicitly call
`Room::setLegacyClientMovementAllowed(true)` for a room that trusts client-declared
movement; the resulting motion is not checked against game geometry. The legacy
movement handler rejects durations outside (0, 0.25] seconds and applies each
accepted move during command dispatch. The 2D movement request uses the same
duration range and applies its vector as one displacement rather than streaming
tick updates.
For these legacy handlers, the server also reserves each requested duration
from a per-user budget shared across 2D, 3D, and transports. The budget refills
at one second per second and holds at most 0.25 seconds, so sending many packets
cannot multiply elapsed movement time. A request that exceeds the available
budget is rejected. The 2D displacement must fit the configured maximum
movement speed for its duration; the 3D velocity is scaled to that speed.

An application room can install a 3D movement input handler with
`Room::setMovementInputHandler`. In that room, the three movement floats are
input direction and jump intent. X and Z are limited to [-1, 1]; Y accepts
[-1, 1] or an integer bit mask from 0 through 7 for game-specific buttons.
A 2D room can install
`Room::setMovement2DInputHandler`; its two movement floats then represent input
components, also limited to [-1, 1], rather than displacement. In both modes,
the client delta time is ignored. The application should record arrival time,
consume input on its server tick, validate input age and game-specific movement
rules, and broadcast positions computed from server state. The input handler
takes precedence over the legacy opt-in. Client position requests
(`room/player_3D/position` and `room/player_2D/position`) are disabled by default;
a room that wants direct client positions must separately call
`setClientPositionAllowed(true)` and install `Room::setPosition3DHandler` or
`Room::setPosition2DHandler` for the dimensions it accepts. The server calls the
handler with the requested in-bounds position (and normalized 3D yaw) before changing the
player state or broadcasting. A missing handler or one that returns false rejects
the request. The application must check travel distance, collision, and any
other game rules using server-owned state; the library cannot infer game
geometry. Configure these handlers before serving room traffic.

The same room setting gates `room/player_2D/position`. This request requires
exactly two finite floats within the configured horizontal bounds. Malformed,
non-finite, or out-of-bounds positions are rejected before the room handler runs.

An application room must install a shot handler to accept shots.
`room/player_3D/shoot` requires three floats for aim direction followed by one
byte for weapon slot (1 primary, 2 secondary, 3 knife) and one action byte
(0 fire, 1 reload). Target ID and damage requests are rejected in every room.
The handler computes the hit and damage from server state; a request with no
valid hit does not damage anyone. Rooms without a shot handler reject shot requests.
Damage also requires an application-installed combat handler to approve the
server-resolved shooter and target. Rooms without a combat handler deny damage.

Client team selection requests are rejected by default. An application must install
`Room::setTeamSelectionHandler` to allow them; the handler can reject a player based
on match state. The server accepts only the `Terrorist` and `Counter Terrorist`
team names from clients. An application can set teams directly on the server.

Client game item creation, status changes, and deletion are disabled by default.
A room can call `setClientGameItemCreationAllowed(true)` to opt into creation,
but each request also requires a `Room::setGameItemCreationHandler` that approves
the actor, position, dimensions, type, status, and asset path against server-owned
state. A missing or rejecting creation handler denies the request before storing
or broadcasting the item.
Status changes and deletion additionally require a
`Room::setGameItemMutationHandler` that approves the actor, item ID, action, and
requested status against server-owned ownership and game state. A missing or
rejecting handler leaves the item unchanged and sends no broadcast. Creation
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

When a pickup handler is installed, it continues to govern the pickup path;
the general item mutation handler does not replace it.

Client room object creation, movement, and deletion are disabled by default.
A room can call `setClientObjectCreationAllowed(true)` to opt into creation,
but each static or moving object request also requires the matching
`Room::setObject2DCreationHandler` or `Room::setObject3DCreationHandler` to
approve the actor, starting position, dimensions, and asset path against
server-owned state. A missing or rejecting creation handler denies the request
before storing or broadcasting the object.
Moving creation also requires `Room::setMovingObject2DCreationHandler` or
`Room::setMovingObject3DCreationHandler`. Configure this policy before serving
room traffic. It receives the actor, start, dimensions, asset path, requested
displacement, duration, movement type, and final bounded destination. The
application must validate the whole path, permitted speed (`displacement / duration`),
destination, ownership, and game rules using server-owned state. An unset or
rejecting moving creation handler denies the request before storage or broadcast,
even if the ordinary creation handler accepts the start. The library's world
clamp is not a game-specific motion or collision rule.
For example, a 2D game can install a handler before admitting players:

```cpp
room->setMovingObject2DCreationHandler(
    [](uint64_t playerId, const Vector2f& start, const Vector2f& dimensions,
       const std::string& asset, const Vector2f& displacement, float duration,
       MovementType type, const Vector2f& destination) {
        // Replace these sample limits with server-owned ownership, map, and
        // collision checks for the game's actual objects.
        constexpr float maxSpeed = 10.0f;
        const float distance = std::hypot(displacement.x, displacement.y);
        return distance / duration <= maxSpeed &&
               gameAllowsPath(playerId, start, destination, dimensions, asset, type);
    });
```

Moving or deleting an existing object additionally requires the matching
`Room::setObject2DMutationHandler` or `Room::setObject3DMutationHandler` to
approve the actor, object ID, action, and bounded destination for moves. A
missing or rejecting handler leaves the object unchanged and sends no
broadcast. Applications must check ownership and game rules in these handlers.
Move handlers run while the room resource lock is held and must not call room
resource accessors. Creation requires finite positions within the configured
world bounds, positive finite dimensions no larger than the horizontal bound, and an
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
and applications can still remove rooms directly on the server. The server
unpublishes an opted-in room, sends a `room/management/remove` response to its
current members, then clears their room IDs. Clients remove the room and its
roster, objects, and chat from local state on this response, clear their current
room ID when it matches, and discard queued events belonging to the room. No
room-info refresh is needed for connected members that receive the response.
Only the room creator may change the room-wide overlap setting through
`room/management/set_overlap`; other members receive an unauthorized result.

Client-requested room creation is denied until the application installs
`RoomStorage::setClientCreationHandler`. The handler receives the authenticated
`User` and proposed `RoomData`; a missing handler, a rejection, or an exception
denies the request. Creation is also capped at 1024 rooms per server and 64
rooms per client by default. The limits are configurable through
`nexilis::server::ServerConfig::getRoomLimits()` as `setMaxRooms` and
`setMaxRoomsPerClient`; requests beyond either limit are rejected. Server-side
`RoomStorage::add` calls are not client requests and bypass this policy.

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

Direct client position requests outside their limits are rejected before room
handlers run. Movement results outside the world bounds are clamped; movement
speeds above the maximum
are scaled down, damage above the per-shot cap is reduced to it, and shots
that arrive sooner than the shooting rate allows are dropped. The shooting
rate applies to fire requests before the room's shot handler runs, including
misses. Reload requests do not consume this cooldown.

`room/communication/broadcast` messages longer than the configured limit, or
arriving faster than the configured broadcast rate, are rejected as invalid
input before they are stored or relayed. The per-client rate limit applies to
the encrypted broadcast form as well, since both forms use the same handler.
Rooms reject client broadcasts until the application installs
`Room::setBroadcastHandler` and approves the sender, payload, and encryption
mode. For encrypted broadcasts, the handler receives base64 ciphertext; an
application that must inspect content should reject that mode or decrypt with
its configured password. Approved broadcasts have `player_authored: true` at the top level of
their JSON messages. Applications must treat the payload as player content;
authoritative game events should use separate server-authored message types.
The server application can call `Command::sendServerRoomEvent(roomId, event,
payload, protocol)` to send a JSON `room/server_event` message with
`action: "emit"`, `room_id`, `event`, and `payload` to current room members.
The event is never accepted from a client command and is not added to broadcast
history. Native clients receive it through `ClientAPI::consumeServerRoomEvents()`;
the `ServerRoomEvent` contains the room ID, event name, and JSON payload.
The C API's `nexilis_client_api_drain_game_events` JSON includes these in a
`server_events` array, separate from the player-authored `chat` array. C# and
Lua clients expose this array through their JSON event drain methods.
Applications must choose a protocol with an authenticated server connection and
validate event names and payloads at their own game boundary. Native clients
reject `room/communication` messages missing `player_authored: true`.
Each room retains at most the 100 most recent broadcasts and replays only those
to joining clients; this history bound is currently fixed in
`nexilis_constants.hh`. Encrypted history retains its
`broadcast_encrypted` action so clients decrypt it on replay. Approved
broadcasts reach every member of the room, including the sender. The server
does not interpret the payload.

`Packet::Room::Player3D::matchAction(api, action)` sends room/player_3D action byte `8` followed by exactly one byte: `0` cancels, `1` begins planting, and `2` begins defusing. The room's application-installed action handler validates the request; rooms without a handler reject it. The server's combat callback must approve hits and can deny participants who are outside an active round. Rooms without a combat callback deny damage.

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

Responses to client commands use the transport that received the request. If
that transport cannot send to a recipient, the send fails instead of retrying
through another registered transport. This also applies to room commands
triggered by a client request; a room member must be reachable on that
transport to receive the event. In particular, a failed TLS send cannot
deliver the response over plaintext UDP or TCP.

`Get::Info::clients` replies as `info_general` with `client_amount`, the number
of connected clients visible to the requesting user. C++ clients expose the
latest value through `ClientAPI::getServerClientCount()`. Joining a room also sends a full leaderboard
seed so team selection screens can show the existing team roster.

The Boost TCP server sends JSON responses terminated by a newline character (`\n`); its client reads until `\n`. The POSIX `af_inet` TCP transport instead prefixes server responses with a 4-byte length. These transports should not be assumed to have interchangeable response framing.

## TLS transport and server identity

Boost TCP uses certificate-based TLS by default. The server has a PEM
certificate and private key that clients do not receive. A client pins the
SHA-256 hash of the certificate's DER SubjectPublicKeyInfo (the public key),
obtained through a trusted channel. It checks the pin and certificate dates
after each TLS handshake, before sending authentication or accepting a response.
The check applies to both the initial and switched TCP connections. A missing
identity or pin fails closed. The connection password remains the separate
application-level client admission secret; it is no longer used as the TLS
server identity. Password-protected mode requires pinned TLS Boost TCP on both
the server and client. Other transports refuse to start in this mode because
they do not implement TLS.

Configure a persistent server identity and give clients only its pin:

```cpp
// Load these PEM strings from server-only files or a secret store.
settings.setTlsIdentity(certificate_pem, private_key_pem);
config.setServerPublicKeySha256(nexilis::tls::publicKeySha256(certificate_pem));
```

Never distribute the private key or derive the pin from a certificate fetched
over the same untrusted connection. Keep the identity stable across restarts;
rotating its public key requires updating clients' trusted pins. The
`examples/messaging` program generates a temporary identity because its server
and clients run in one process.

To use plaintext Boost TCP in skip mode, both sides must explicitly opt out:

- `nexilis::server::ServerConfig::setTls(false)` on the server.
- `nexilis::client::ClientConfig::setTls(false)` (or `nexilis_client_config_set_tls` / the C# `ClientConfig.SetTls` / Lua `tls = false`) on every client.

Configuration details:

- The server refuses TLS connections if its certificate or private key is missing, invalid, or mismatched. The client rejects a missing or malformed 64-digit hex pin before connecting.
- The client rejects a TLS peer with a different public key, no certificate, or a certificate outside its validity period. The server public key is independent of the connection password.
- If only one side enables TLS, the handshake fails rather than silently downgrading to plaintext.
- Password-protected Boost TCP refuses to start without TLS; the client also requires a valid server public-key pin. Password-protected clients and servers refuse plaintext IPv4, Unix socket, and raw UDP transports. Boost UDP accepts token-authenticated session traffic after TLS authentication, but does not encrypt it.
- The Boost TCP client captures its TLS setting when the protocol object is constructed; changing the client API setting afterward does not downgrade an existing protocol object.
- The handshake happens lazily on the server before the first message is read or written. The client completes the TLS handshake and verifies the pin immediately after each TCP connect.

## Room creation

Room creation requests use a 3-byte room-management command header, followed by a
1-byte room context, a 2-byte unsigned maximum player count in the configured
wire byte order, and a room name of 1–64 bytes (a single space is rejected).
The server checks the complete payload length before indexing or copying its
fields, rejecting truncated and oversized requests. It accepts capacities from 1 to 64,
includes `max_size` in the creation response and room listing, and rejects joins
when the room is full. Applications must install a
`RoomStorage::setClientCreationHandler` to permit client creation at all, and
can install a `RoomStorage::setCreateHandler` callback to configure each new
room before the server acknowledges creation. Each client may belong to one
room at a time. A join while already in a room is rejected; the client must
leave before joining another room.
Rooms are private by default. Without `Room::setAdmissionHandler`, only the
creator and users who already joined may discover and join a room. Servers can
install an admission handler to implement any other policy; the handler
receives the server-side `User` and returns `true` to allow access. An
explicitly permissive handler makes a room public. A denied user cannot join or
see that room in `info_rooms`, `info_general`, or `info_clients`; an exception
in the handler denies access. Set the admission handler in
`RoomStorage::setCreateHandler` before room creation is acknowledged. Changing
the handler later does not evict users who already joined, and handlers are not
called for server-side `RoomStorage::add` calls.

## Security notes

Command-channel passphrase authentication (`authentication`, action `2`) is
rejected; clients authenticate during the initial connection handshake. Root
authentication (`authentication`, action `1`) requires an established
`password_protected` session with common access over TLS Boost TCP and a
configured root password. Five failed root attempts from one source address
block further attempts from that address for ten minutes, including after a
reconnect. The attempt state is scoped to the server protocol instance and
resets when that instance is replaced. At most 4096 source addresses with failed
attempts are tracked. A correct root password from a new address can still
authenticate when the table is full. A new failed attempt evicts the oldest
non-blocked failure record, or the oldest blocked record if all are blocked.
The server logs when the table reaches 75% capacity and during sustained
eviction. Distributed guessing and traffic floods still require external
controls at the deployment boundary.

The initial connection handshake for `password_protected` mode is separately
rate-limited: five failed passphrase attempts from one source address block
further handshake attempts from that address for ten minutes, including after a
reconnect. At most 4096 source addresses are tracked; while that limit is full,
new addresses are denied handshake attempts. A successful authentication clears
the recorded failures for the address. Like the root limiter, this state is
scoped to the server protocol instance and resets when that instance is
replaced.

New registrations in both `skip` and `password_protected` modes are limited to
20 successful handshakes per source address per minute. Only published clients
count toward this limit; rejected admissions do not consume registration table
entries. The table tracks at most 4096 addresses and evicts its oldest entry
when full. A new `skip` user must send the canonical 19-byte
`getting/client_id` handshake. Unknown or malformed requests do not allocate a
user. The shared client registry admits at most `NEXILIS_DEFAULT_MAX_CLIENTS`
live users (currently 1024) and limits each source address to eight live
sessions by default. Configure lower deployment limits with
`ServerConfig::setMaxClients` and `ServerConfig::setMaxSessionsPerAddress`;
zero refuses new clients. The compile-time client maximum still applies.
Admission limits are checked together under the registry lock, so concurrent
handshakes cannot exceed them. Slots are released when users disconnect, a
pending handshake expires, or an idle datagram transport expires. Connected
stream clients remain registered while their connection stays open.

`ClientAPI::setMessageEncryption(true)` affects room broadcasts. It does not encrypt othercast or unicast messages. The message key is derived from the password and the room ID; the server holds the password, so this feature does **not** hide message contents from the server.

If encryption is enabled but the password is missing, key derivation fails, or
encryption fails, the client packet builder returns no packet. Receivers reject
encrypted broadcasts that cannot be decrypted instead of exposing ciphertext
as application content. This option does not grant server authority to player
messages or confidentiality from the server. Password-protected sessions
require pinned TLS Boost TCP.

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
horizontal position. The room must install `setDimension2DHandler` or
`setDimension3DHandler` to approve each client dimension change against its
server-owned character state. Missing or rejecting handlers leave dimensions
unchanged and send no broadcast.

2D visual shot requests require exactly two finite direction floats; zero
vectors are rejected. The server normalizes accepted directions using a wider
length calculation to handle extreme finite float values safely. The room must
install `setShot2DHandler` to authorize each visual shot against its weapon and
ammo state. The server also applies the configured shooting rate limit. This
visual command does not resolve hits or damage. Audio-event payloads require
exactly one sound byte and three finite position floats within the configured
world bounds; trailing bytes are rejected. The room must install
`setAudioEventHandler` to approve the sound and position against server-owned
game state before relay. Missing or rejecting handlers suppress those events.

These checks bound numerical inputs. Applications must decide which client
requests represent legitimate character, weapon, and sound events. Configure
handlers before serving room traffic.
