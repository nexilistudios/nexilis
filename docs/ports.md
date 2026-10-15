# Network ports

Nexilis uses these server listening ports by default:

| Transport | Port | Purpose |
| --- | ---: | --- |
| Boost TCP | 54200/TCP | Initial connection and authentication |
| Boost UDP | 54201/UDP | UDP messages |
| Boost TCP session | 54202/TCP | Authenticated connection after the port switch |
| af_inet TCP | 54300/TCP | TCP messages |
| af_inet UDP | 54301/UDP | UDP messages |

Open the ports for the transports your application starts. The Boost TCP server
uses both 54200/TCP and 54202/TCP. Clients use the same defaults without reading
port files. The server still sends the session port to the client as part of the
existing protocol.

Applications can explicitly configure different af_inet TCP/UDP server ports and
matching client ports. Client port settings also allow a different Boost UDP
destination. Keep both ends in sync when overriding a port. Outbound client
sockets can have operating-system-assigned local source ports; these are not
server listening ports.

Call `Ports::setBoostTCPPort`, `Ports::setBoostUDPPort`, and
`Ports::setBoostTCPSessionPort` before constructing the corresponding Boost
protocol objects to override their process-wide defaults. The Boost client only
needs the authentication TCP and UDP overrides: it learns the TCP session port
from the server during authentication.

## Test ports

The network tests use separate ports so they can run while an application is
listening on Nexilis's default ports:

| Test suite | Boost TCP | Boost UDP | Boost TCP session | af_inet TCP | af_inet UDP |
| --- | ---: | ---: | ---: | ---: | ---: |
| Native C++ | 55200 | 55201 | 55202 | 55210 | 55211 |
| Premake | 55300 | 55301 | 55302 | — | — |
| LuaJIT | 55400 | 55401 | 55402 | — | — |
| Example runner | 55500 | 55501 | 55502 | — | — |
| C# connection failure test | 55600 | — | — | — | — |

`make test-examples` passes `--test-ports` to examples; running an example
directly still uses the library defaults. C and Lua clients can set the
process-wide Boost ports through `nexilis_set_boost_tcp_port`,
`nexilis_set_boost_udp_port`, and `nexilis_set_boost_tcp_session_port` before
constructing transport objects. These setters mirror the C++ `Ports` API.
