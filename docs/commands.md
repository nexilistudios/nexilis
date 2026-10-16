# Commands and packets

Nexilis routes every client/server operation through one shared template so that
the wire format is authored once and the two sides stay in sync. The server calls
these operations **commands**; the client calls the outgoing byte builders
**packages** (exposed to C++ callers as `nexilis::client::Packet`).

## The shared template (CRTP)

`nexilis/include/nexilis/command_packet_base.hh` declares one template:

```cpp
template <typename Impl>
class CommandPacketBase
{
public:
    struct Set { struct General { ... }; struct Protocol { ... }; };
    struct Get { struct General { ... }; struct Info { ... }; };
    struct Room
    {
        struct Management { ... };
        struct Communicate { ... };
        struct Player2D { ... };
        struct Object2D { ... };
        struct Player3D { ... };
        struct Object3D { ... };
        struct GameItem { ... };
    };
};
```

The nested `struct`s group related operations and act as a namespace (`Set`,
`Get`, `Room::Management`, and so on). Each leaf is a small forwarding function
that calls back into the implementation type:

```cpp
template <typename... Args>
static auto join(Args&&... args)
{
    return Impl::room_management_join(std::forward<Args>(args)...);
}
```

This is the Curiously Recurring Template Pattern (CRTP)-style static dispatch:
`CommandPacketBase` is parameterized by its implementation type `Impl` and calls
`Impl`'s static functions. The implementation is selected at compile time, so
there are no virtual functions, vtables, or runtime indirection — a call such as
`Packet::Room::Management::join(...)` compiles down to a direct
`ClientImpl::room_management_join(...)` call. The name of the implementation
struct appears as the template argument rather than through inheritance, which
is the "curiously recurring" link between the base and its implementation.

## Server: `ServerImpl` and `Commands`

- `nexilis/include/nexilis/server/command/commands.hh` declares `struct ServerImpl`
  with one static handler per operation, all taking the same argument bundle:

  ```cpp
  static CommandResult room_management_join(const DefaultArgs& args);
  ```

- The same header exposes the server dispatch surface:

  ```cpp
  using Commands = CommandPacketBase<ServerImpl>;
  ```

- `DefaultArgs` carries the `User`, the `NXProtocol`, the raw `nx_data` command
  bytes, and the message id.
- Handlers are implemented one per file under
  `nexilis/src/nexilis/server/command/commands/...`, for example
  `commands/room/management/join.cc`.
- Incoming bytes are decoded by `server::Command::read()` in
  `nexilis/src/nexilis/server/command.cc`. It switches on the command bytes and
  calls the matching `ServerImpl::...` handler with `DefaultArgs`, returning a
  `CommandResult` that classifies the outcome.

## Client: `ClientImpl` and `Packet`

- `nexilis/include/nexilis/client/packet.hh` declares `struct ClientImpl` with one
  static builder per operation:

  ```cpp
  static nx_data room_management_join(ClientAPI& api, uint64_t roomId);
  ```

- The same header exposes the client builder surface:

  ```cpp
  using Packet = CommandPacketBase<ClientImpl>;
  ```

- Builders are implemented in `nexilis/src/nexilis/client/packet.cc` and return
  the command bytes (`nx_data`) that the transport frames and sends.
- Callers use the same nested scopes as the server:

  ```cpp
  client.sendMessage(nexilis::client::Packet::Room::Management::join(api, room_id));
  client.sendMessage(nexilis::client::Packet::Room::Communicate::broadcast(api, "hello"));
  ```

The server and client implementations are independent `CommandPacketBase`
instantiations, so `nexilis::client::Packet` and `nexilis::server::Commands` are
different types even though both expose the same nested scopes.

## Adding an operation

1. Declare the static function on `ServerImpl` (`commands.hh`) or `ClientImpl`
   (`packet.hh`) under a `// Group::Subgroup` comment.
2. Forward it from the matching scope in `command_packet_base.hh`.
3. Implement the server handler in
   `nexilis/src/nexilis/server/command/commands/...` (returning `CommandResult`),
   or the client builder in `nexilis/src/nexilis/client/packet.cc` (returning
   `nx_data`).
4. For a server-side operation, add the dispatch branch in
   `server::Command::read()` in `nexilis/src/nexilis/server/command.cc`.

The on-wire byte layout that these calls produce and consume is described in the
[message protocol](server_message.md).

## Authentication commands

The initial connection handshake handles the server passphrase. The legacy
`authentication` action `2` command is rejected on established sessions. Root
access uses `authentication` action `1` only after password authentication on
TLS-PSK Boost TCP; failed attempts are limited by source address as described in
the [security notes](server_message.md#security-notes). Other transports and
`skip` mode cannot use this command to gain root access.
