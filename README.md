# Nexilis

Nexilis is a C++20 client and server library for multiplayer applications. It provides TCP and UDP transports, rooms and messaging, a C API, and C# bindings. See [VERSION.txt](VERSION.txt) for the current version and [weekly releases](docs/releases.md) for the release process.

## Build and try an example

Prerequisites: CMake 3.25 or newer, a C++20 compiler, OpenSSL development headers and libraries (1.1.1 or newer), and Git. A local build uses the Boost submodule; clone with submodules or initialize them before configuring. The project is currently tested primarily on Linux.

```sh
git clone --recurse-submodules https://github.com/NexilisLib/nexilis.git
cd nexilis
cmake -S examples/skip_auth -B examples/skip_auth/build
cmake --build examples/skip_auth/build
./examples/skip_auth/build/skip_auth
```

For an existing clone, run `git submodule update --init --recursive` first. The `skip_auth` example starts a server and two clients on localhost and shows room messaging. It deliberately accepts clients without a password; use it only for local experimentation. See [examples/messaging](examples/messaging/main.cc) for password authentication and optional TLS on the Boost TCP transport.

To build the native libraries directly:

```sh
cmake -S nexilis -B nexilis/build -DCMAKE_BUILD_TYPE=Release
cmake --build nexilis/build --parallel
```

On Linux with the MinGW-w64 cross compiler and Windows OpenSSL development libraries installed, run `make build-windows` to build Windows DLLs in `nexilis/build-windows`. It uses `nexilis/cmake/mingw_toolchain.cmake` and leaves native build directories intact.

`make install` builds and installs into `nexilis/build/install` within the checkout. The current install target does **not** install public headers, so it is not yet a complete SDK for downstream projects.

## Documentation

- [Quick start](docs/quickstart.md), [documentation index](docs/README.md), and [dependencies](docs/dependencies.md)
- [Client/server message format](docs/server_message.md)
- [C# bindings](bindings/README.md)
- [Examples](examples/) and [test commands](docs/README.md#testing)
- [Contributing](CONTRIBUTING.md)

## Security

Network traffic is plaintext by default. TLS with a pre-shared key is optional for the Boost TCP client/server pair and must be enabled on both sides. The optional encrypted room broadcast uses a key derived from the server authentication password, which the server also knows; it does not provide confidentiality from the server. See [protocol and security notes](docs/server_message.md#security-notes) before deploying outside a trusted environment.

## License

Project source files identify the GNU Lesser General Public License, version 3 or later. See [LICENSE](LICENSE). The Boost submodule has its own licenses.
