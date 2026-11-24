# Nexilis bindings

The LuaJIT client binding is documented in [`bindings/lua/README.md`](lua/README.md).

## C#

The C# wrapper is in [`bindings/csharp`](csharp/) and targets .NET Standard 2.0. Build it with a .NET SDK:

```sh
dotnet build bindings/csharp/Nexilis.csproj
```

Run that command from the repository root. The wrapper calls the native `nexilisc` shared library through P/Invoke; the native `nexilis` library and its runtime dependencies must also be available to the dynamic loader. Building the C# assembly alone does not produce a standalone package.

`make install-csharp` runs [`scripts/install_csharp.py`](../scripts/install_csharp.py)
to build the native libraries and place the C# assembly and native libraries under
`dist/` by default. It also copies dynamically linked Boost libraries, including
Boost.JSON, beside the native libraries under `dist/<platform>/<arch>/`. Run it
from the repository root after initializing the Boost module submodules. The script
currently selects the host platform, so its `--arch` label does not
cross-compile binaries. OpenSSL and the C/C++ runtime remain system
dependencies. On Linux, add `dist/Linux/<arch>/` to the runtime library search
path if a copied Boost library has further shared-library dependencies.

`NxLogger.Setup` replaces its previous callback, and each `NxLogger` owns its
native logger. Keep a `Logger.FunctionHandler` alive while its callback is
registered; after adding it to a `Logger.Logger`, use `GetId()` to remove the
native handler before disposing the managed wrapper.

To run the C# test project after the native libraries are available, use `make test-csharp`.

Boost TCP clients require the intended server's public-key SHA-256 pin when
TLS is enabled. Set it with `ClientConfig.SetServerPublicKeySha256` or the
`NexilisClient.ServerPublicKeySha256` property. Obtain the pin through a
trusted channel; the connection password alone does not authenticate the
server. See [TLS transport setup](../docs/server_message.md#tls-transport-and-server-identity).

Password-protected mode requires pinned TLS Boost TCP. The high-level C# client
sends packets selected for UDP over TCP in that mode. UDP and other plaintext
transports are available only in skip mode.
Protected servers require account authentication. Set
`ClientConfig.SetAccountCredentials` or `NexilisClient.AccountId` and
`AccountCredential` before connecting.

`ClientAPI.DrainGameEventsJson()` includes `server_events` separately from the
player-authored `chat` array. Each server event has a decimal string `room_id`,
an `event` name, and a JSON `payload`. Use only `server_events` for authoritative
game state; chat and broadcast history remain player content. Native strings
returned through C# getters are copied as UTF-8 and freed through the C API.

For server-authoritative 3D shots, use `NexilisClient.Fire(x, y, z, slot)`.
The former `Shoot(targetId, damage)` request is rejected by servers.

Movement packets from C, C#, and Lua clients keep their existing wire format.
Servers reject them in a room unless the application installs a movement input
handler or explicitly enables legacy client movement for that room. See the
[movement protocol](../docs/server_message.md#application-match-extension).

`Room.GetClients()` and `ClientAPI.GetClientFromClientId()` return independently
owned session snapshots. Dispose each returned `ClientSession` after use.
Snapshots remain valid after room clients change or the room is destroyed;
setters modify the snapshot, and a new lookup is needed to read current room
state. The C API's `nexilis_client_session_clone` provides the same copy behavior;
release its result with `nexilis_client_session_destroy`. Existing C room client
arrays still contain borrowed sessions and are freed with
`nexilis_room_free_client_array`.
