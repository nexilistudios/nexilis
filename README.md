# Nexilis

Nexilis is a C++20 client and server library for multiplayer applications. It provides TCP and UDP transports, rooms and messaging, a C API, and C# bindings. See [VERSION.txt](VERSION.txt) for the current version and [weekly releases](docs/releases.md) for the release process.

## Documentation

- [Quick start](docs/quickstart.md), [documentation index](docs/README.md), and [dependencies](docs/dependencies.md)
- [Client/server message format](docs/server_message.md)
- [Commands and packets](docs/commands.md)
- [Network ports](docs/ports.md)
- [C# bindings](bindings/README.md)
- [Examples](examples/) and [test commands](docs/README.md#testing)
- [Contributing](CONTRIBUTING.md)

## Security

Boost TCP uses certificate-based TLS by default. The server configures a certificate and private key with `setTlsIdentity`; each client pins that server's public-key SHA-256 with `setServerPublicKeySha256`. Password-protected admission requires both the shared application password and a server-validated private account credential. Missing or mismatched TLS identity configuration rejects the connection. Applications using skip mode can explicitly call `setTls(false)` on both sides for plaintext TCP, as the passwordless `skip_auth` example does. Password-protected sessions are established over pinned TLS Boost TCP; Boost UDP can then carry authenticated but plaintext gameplay traffic using the session token. Direct UDP admission is disabled in every mode, so skip-mode UDP cannot disclose a bearer token. Other plaintext TCP and Unix socket transports refuse to start in password-protected mode. The optional encrypted room broadcast uses a key derived from the server authentication password, which the server also knows; it does not provide confidentiality from the server. See [protocol and security notes](docs/server_message.md#security-notes) before deploying outside a trusted environment.

## License

Project source files identify the GNU Lesser General Public License, version 3 or later. See [LICENSE](LICENSE). The Boost module submodules have their own licenses.
