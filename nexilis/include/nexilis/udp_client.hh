/* Copyright (C) 2026 Valtteri Viirret
   This file is part of the Nexilis Project.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this file.  If not, see <https://gnu.org>. */

#ifndef NEXILIS_UDP_CLIENT_HH
#define NEXILIS_UDP_CLIENT_HH

#include <nexilis/client/protocol/nxboost/udp_client.hh>
#include <nexilis/nx_protocol.hh>

namespace nexilis
{

class UDPClient
{
public:
    /// Constructor.
    explicit UDPClient(client::ClientAPI& api);

    /// Move constructor.
    UDPClient(UDPClient&& other) noexcept;

    /// Move assignment operator.
    UDPClient& operator=(UDPClient&& other) noexcept;

    /// Deleted copy constructor.
    UDPClient(const UDPClient&) = delete;

    /// Deleted copy assignment operator.
    UDPClient& operator=(const UDPClient&) = delete;

    /// Get the NXProtocol::Type.
    static NXProtocol::Type getType()
    {
        return NXProtocol::Type::BOOST_UDP_CLIENT;
    }

    /// Start running the UDP client.
    void start();

    /// Stop running the UDP client.
    void stop();

    /// Send nexilis message.
    void sendMessage(const nx_data& message);

    /// Send nexilis message with a callback.
    void sendMessage(const nx_data& message, const std::function<void()>& callback);

    /// Send async nexilis message.
    std::future<void> sendMessageAsync(const nx_data& message);

private:
    client::nxboost::UDPClient m_udpClient;
};

} // namespace nexilis

#endif
