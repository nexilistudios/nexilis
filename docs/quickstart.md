# Nexilis quick start

Get a server and two clients exchanging room messages on localhost in a few
minutes, then wire the library into your own project. The current version is in
[VERSION.txt](../VERSION.txt).

## 1. Prerequisites

- CMake 3.25 or newer and a C++20 compiler
- OpenSSL development headers and libraries (1.1.1 or newer)
- Git, with the Boost submodule initialized for local builds

```sh
git clone --recurse-submodules https://github.com/NexilisLib/nexilis.git
cd nexilis
```

For an existing clone, run `git submodule update --init --recursive` first. See
[dependencies](dependencies.md) for how system Boost and submodule Boost are
selected.

## 2. Run your first example

The `skip_auth` example starts a server and two clients on localhost and shows
room messaging:

```sh
cmake -S examples/skip_auth -B examples/skip_auth/build
cmake --build examples/skip_auth/build
./examples/skip_auth/build/skip_auth
```

You should see Alice create a chat room, Bob discover and join it, and both
print every broadcast. The example accepts clients without a password
(`AuthenticationMode::skip`), so use it only for local experimentation.

Other examples in [`examples/`](../examples/):

| Example | Shows |
| --- | --- |
| `messaging` | Password authentication, optional TLS, optional message encryption |
| `boost` | Boost TCP and UDP transports side by side |
| `unix_stream_server`, `unix_stream_client` | Unix stream transport with a TOML config |

Build them the same way, or run `make test-examples` from the repository root.

## 3. Build the libraries

To build the native libraries for your own project:

```sh
cmake -S nexilis -B nexilis/build -DCMAKE_BUILD_TYPE=Release
cmake --build nexilis/build --parallel
```

This produces `libnexilis` and the C API library `libnexilisc` in
`nexilis/build`. `make install` installs into `nexilis/build/install`, but the
install target does **not** currently copy public headers, so it is not yet a
standalone SDK. Until that is fixed, point your build at the source checkout's
`nexilis/include/` and the libraries in `nexilis/build/`.

## 4. The core API in 30 lines

Every program uses the same four pieces: a `ProtocolManager`, a config, a
`ClientAPI` for state, and a protocol object that sends packets.

```cpp
#include <nexilis/client/client_api.hh>
#include <nexilis/client/packet.hh>
#include <nexilis/client/protocol/nxboost/tcp_client.hh>
#include <nexilis/protocol_manager.hh>
#include <nexilis/server/protocol/nxboost/tcp_server.hh>

nexilis::ProtocolManager protocol_manager;

// Server: skip authentication for a local experiment.
nexilis::server::ServerConfig settings;
settings.setMode(nexilis::server::AuthenticationMode::skip);
auto server = protocol_manager.createProtocol<nexilis::server::nxboost::TCPServer>(settings);
server.start();

// Client.
nexilis::client::ClientConfig config;
config.setBoostTCPAddress("127.0.0.1");
config.setMode(nexilis::server::AuthenticationMode::skip);

nexilis::client::ClientAPI api(config);
auto client = protocol_manager.createProtocol<nexilis::client::nxboost::TCPClient>(api);
client.start();

client.sendMessage(nexilis::client::Packet::Set::General::username(api, "Alice"));
client.sendMessage(nexilis::client::Packet::Room::Management::create(api, "chat"));

// ...wait for api.getActiveRooms(), then join and broadcast:
client.sendMessage(nexilis::client::Packet::Room::Management::join(api, room_id));
client.sendMessage(nexilis::client::Packet::Room::Communicate::broadcast(api, "hello"));

client.stop();
server.stop();
```

Things worth knowing before you go further:

- **State is polled, not pushed.** `ClientAPI` holds the current snapshot:
  `getActiveRooms()`, `getRoom(id)`, `getClientId()`, `clientInRoom()`.
  Messages accumulate in `room.getMessages()`; track how many you have printed
  and read the rest. The examples use a small `waitFor` polling loop for this.
- **Senders are visible.** `message.getClient()` gives the sending client, or
  `nullptr` if unknown; `getUsername()` gives its name.
- **Broadcasts reach the sender too.** Everyone in the room, including you,
  receives the message.
- **TLS and encryption are opt-in.** Traffic is plaintext by default. TLS needs
  `setTls(true)` on both the server and client configs. Room message encryption
  is enabled per client with `api.setMessageEncryption(true)`. Read the
  [security notes](server_message.md#security-notes) before deploying outside a
  trusted environment.
- **Authentication modes.** `AuthenticationMode::skip` accepts anyone;
  `password_protected` requires `setPassphrase` on the server and
  `setPassword` on each client. See `examples/messaging`.

## 5. Other transports

Create a different protocol type on the same `ProtocolManager`:

```cpp
auto udp_server = protocol_manager.createProtocol<nexilis::server::nxboost::UDPServer>(settings);
udp_client_config.setBoostUDPAddress("127.0.0.1");
auto udp_client = protocol_manager.createProtocol<nexilis::client::nxboost::UDPClient>(udp_api);
```

Reliable traffic (chat, room management) goes over TCP; position, movement, and
audio typically ride UDP. See [`examples/boost/main.cc`](../examples/boost/main.cc)
and the [message protocol](server_message.md) for framing details.

## 6. Bindings

| Language | Entry point |
| --- | --- |
| C | [`nexilis/include/nexilisc/`](../nexilis/include/nexilisc/) |
| C# | [`bindings/README.md`](../bindings/README.md) — `dotnet build bindings/csharp/Nexilis.csproj`, needs .NET and the native libraries on the loader path |
| LuaJIT | [`bindings/lua/README.md`](../bindings/lua/README.md) — set `NEXILISC_LIBRARY` and `LUA_PATH`, then `require("nexilis")` |

The LuaJIT client covers connection, rooms, position and movement, chat, and
event draining; the README has a full snippet.

## 7. Tests and next reads

```sh
make test-cpp      # C++ suite (needs GoogleTest)
make test-c        # C API suite
make test-examples # build and run the examples
make test          # everything, including Premake, Lua, and C#
```

Then read:

- [Documentation index](README.md) — structure and testing
- [Commands and packets](commands.md) — how `Packet`/`Commands` dispatch works
- [Message protocol](server_message.md) — packet format and security notes
- [Dependencies](dependencies.md) — Boost, OpenSSL, GoogleTest
- [Bindings](../bindings/README.md) — C# and LuaJIT usage
