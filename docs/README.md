# Nexilis documentation

Nexilis is a C++20 client/server library with C, C#, and LuaJIT interfaces. Start with the [quick start](quickstart.md) for a first running example and an API walkthrough, or the [repository README](../README.md) for a local build.

## Project structure

| Path | Contents |
| --- | --- |
| `nexilis/` | Native C++ library and C API |
| `bindings/csharp/` | C# wrapper for the native library |
| `bindings/lua/` | LuaJIT client wrapper for the C API |
| `examples/` | Standalone C++ examples |
| `tests/` | Native, C API, and C# tests |
| `scripts/` | Build and quality-check scripts |

See [dependencies](dependencies.md), [network ports](ports.md), the [message protocol](server_message.md), [commands and packets](commands.md), and the [bindings guide](../bindings/README.md) for details.

## Testing

The native test suites need GoogleTest and a configured Nexilis build. After `make install`, run `make test-cpp` and `make test-c`. C# tests also require the .NET SDK; run `make test-csharp`. `make test-lua` requires LuaJIT and runs a local TCP/UDP server. `make test-examples` builds and runs the examples. `make test` runs all suites, including Premake, which requires Premake 5.

The CI workflow in [`.github/workflows/ci.yml`](../.github/workflows/ci.yml) shows the dependencies used for each check.

See [weekly releases](releases.md) for automated patch versioning and Doxygen
website publishing.

The Boost TCP client resolves server hostnames for its authentication and session connections. The Boost UDP client also resolves IPv4 hostnames before sending gameplay packets. A failed TCP session-port connection changes the client protocol status to `error` so callers can report the failure.

Server applications can call `nexilis::server::installSignalHandlers()` and then
`nexilis::server::waitForShutdown()` for graceful shutdown. On Windows this handles
Ctrl+C, Ctrl+Break, console close, logoff, and system shutdown control events. On POSIX
systems it handles SIGINT, SIGTERM, and SIGHUP. A shutdown callback registered
with `setShutdownCallback()` runs once per request on the thread calling
`waitForShutdown()`; call `clearShutdownRequested()` before waiting for another
request. Console close and system shutdown events may impose a platform deadline
on cleanup.
