# Network ports

Nexilis uses these fixed server listening ports by default:

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
