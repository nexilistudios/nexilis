/* Copyright (C) 2026 Valtteri Viirret
   This file is part of the Nexilis Project.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this file.  If not, see <https://gnu.org>. */

#include <nexilis/nx_protocol.hh>

namespace nexilis
{

NXProtocol::Mode NXProtocol::getMode(NXProtocol::Type type)
{
    switch (type)
    {
        case NXProtocol::Type::AF_INET_UDP_SERVER:
        case NXProtocol::Type::AF_INET_TCP_SERVER:
        case NXProtocol::Type::BOOST_UDP_SERVER:
        case NXProtocol::Type::BOOST_TCP_SERVER:
        case NXProtocol::Type::AF_UNIX_SOCK_DGRAM_SERVER:
        case NXProtocol::Type::AF_UNIX_SOCK_STREAM_SERVER:
            return NXProtocol::Mode::SERVER;

        case NXProtocol::Type::AF_INET_UDP_CLIENT:
        case NXProtocol::Type::AF_INET_TCP_CLIENT:
        case NXProtocol::Type::BOOST_UDP_CLIENT:
        case NXProtocol::Type::BOOST_TCP_CLIENT:
        case NXProtocol::Type::AF_UNIX_SOCK_DGRAM_CLIENT:
        case NXProtocol::Type::AF_UNIX_SOCK_STREAM_CLIENT:
            return NXProtocol::Mode::CLIENT;

        default:
            return NXProtocol::Mode::UNKNOWN;
    }
}

std::string NXProtocol::typeToString(Type type)
{
    switch (type)
    {
        case Type::AF_INET_UDP_SERVER:
            return "af_inet::UDPServer";
        case Type::AF_INET_UDP_CLIENT:
            return "af_inet::UDPClient";
        case Type::AF_INET_TCP_SERVER:
            return "af_inet::TCPServer";
        case Type::AF_INET_TCP_CLIENT:
            return "af_inet::TCPClient";

        case Type::BOOST_UDP_SERVER:
            return "boost::UDPServer";
        case Type::BOOST_UDP_CLIENT:
            return "boost::UDPClient";
        case Type::BOOST_TCP_SERVER:
            return "boost::TCPServer";
        case Type::BOOST_TCP_CLIENT:
            return "boost::TCPClient";

        case Type::AF_UNIX_SOCK_DGRAM_CLIENT:
            return "af_unix::DgramClient";
        case Type::AF_UNIX_SOCK_DGRAM_SERVER:
            return "af_unix::DgramServer";
        case Type::AF_UNIX_SOCK_STREAM_CLIENT:
            return "af_unix::sock_stream::Client";
        case Type::AF_UNIX_SOCK_STREAM_SERVER:
            return "af_unix::sock_stream::Server";

        default:
            return "UNDEFINED";
    }
}

} // namespace nexilis