# Dependencies

The native library requires CMake 3.25 or newer, a C++20 compiler, OpenSSL 1.1.1 or newer, and Boost. `nexilis/CMakeLists.txt` builds against the Boost submodule for local builds. In CI it uses a system Boost installation with the JSON component (Boost 1.75 or newer).

OpenSSL is a required build dependency even when the optional encryption features are disabled. The native libraries link to `OpenSSL::SSL`. Boost.JSON is a compiled dependency; the local build creates it from the submodule.

The CMake install target does not include public headers. Its package config also records absolute Boost include paths from the source checkout when submodules are used, so the installed package alone cannot be used by downstream projects or reliably moved to another machine.

For a local checkout, initialize the submodule:

```sh
git submodule update --init --recursive
```

On Ubuntu, install the basic build tools and OpenSSL development package:

```sh
sudo apt install cmake g++ libssl-dev
```

For a Windows cross build from Linux, install the MinGW-w64 C and C++ compilers, windres, and OpenSSL development libraries for the MinGW target. Run `make build-windows` for DLLs. Set `OPENSSL_ROOT_DIR` to the Windows OpenSSL installation if CMake cannot find it. The host's `libssl-dev` does not provide Windows libraries. Windows builds provide the Boost.Asio TCP and UDP backends; the native `af_inet` and Unix socket backends require POSIX APIs.

CI or another build using system Boost also needs `libboost-json-dev`. On Arch Linux, the corresponding packages include `cmake`, `gcc`, `openssl`, and `boost`.

The C# wrapper targets .NET Standard 2.0 and needs a .NET SDK to build. It calls the native `nexilisc` shared library at runtime; deploying only `Nexilis.dll` is insufficient. Native tests require GoogleTest, and the Premake test requires Premake 5.

The Lua client binding requires LuaJIT and the matching native shared libraries. Its current test and timing code targets 64 bit Linux. Run `make test-lua` to exercise it against a local TCP/UDP server.
