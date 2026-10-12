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

#ifndef NEXILIS_TCP_CLIENT_HH
#define NEXILIS_TCP_CLIENT_HH

#include <nexilis/client/protocol/nxboost/tcp_client.hh>
#include <nexilis/nx_protocol.hh>

namespace nexilis
{

class TCPClient
{
public:
    /// Constructor.
    explicit TCPClient(client::ClientAPI& client_api);

    /// Move constructor.
    TCPClient(TCPClient&& other) noexcept;

    /// Move assignment operator.
    TCPClient& operator=(TCPClient&& other) noexcept;

    /// Get the NXProtocol::Type.
    static NXProtocol::Type getType()
    {
        return NXProtocol::Type::BOOST_TCP_CLIENT;
    }

    /// Start running nexilis tcp client.
    void start();

    /// Stop running nexilis tcp client.
    void stop();

    /// Send nexilis message
    void sendMessage(const nx_data& message);

    /// Send nexilis message with custom callback.
    void sendMessage(const nx_data& message,
                     const std::function<void()>& callback);

    std::future<void> sendMessageAsync(const nexilis::nx_data& message);

    nexilis::client::ProtocolStatus getProtocolStatus();

private:
    nexilis::client::nxboost::TCPClient m_tcpClient;
};

} // namespace nexilis

#endif
