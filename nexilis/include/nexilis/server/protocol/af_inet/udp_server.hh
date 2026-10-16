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

#ifndef NEXILIS_AF_INET_UDP_SERVER_HH
#define NEXILIS_AF_INET_UDP_SERVER_HH

#include <nexilis/nx_protocol.hh>
#include <nexilis/server/server_config.hh>
#include <nexilis/server/server_protocol.hh>

#include <netinet/in.h>

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace nexilis::server::af_inet
{

/// AF_INET UDP server protocol using raw sockets.
/// Works similarly to the boost UDP server but over a plain UDP socket.
class UDPServer : public NXProtocol,
                  public ServerProtocol
{
public:
    /// Constructor.
    /// \param settings The server settings.
    /// \param port The UDP port to bind to. Zero selects the default port.
    UDPServer(const ServerConfig& settings, uint16_t port);

    /// Destructor.
    ~UDPServer();

    /// Move constructor.
    UDPServer(UDPServer&& other);

    /// Move assignment operator.
    UDPServer& operator=(UDPServer&& other);

    /// Deleted copy constructor.
    UDPServer(const UDPServer& other) = delete;

    /// Deleted copy assignment operator.
    UDPServer& operator=(const UDPServer& other) = delete;

    /// NXProtocol::start() implementation.
    void start() override;

    /// NXProtocol::stop() implementation.
    void stop() override;

    /// NXProtocol::getType() implementation.
    NXProtocol::Type getType() override
    {
        return NXProtocol::Type::AF_INET_UDP_SERVER;
    }

    /// Get the port the server is bound to.
    uint16_t getPort() const
    {
        return m_port;
    }

private:
    void createSocket();
    void bindSocket();
    void receiveFromClients();
    void sendToClient(const sockaddr_in& clientAddress, const nx_data& message);

private:
    uint16_t m_port;
    int m_serverSocket = -1;
    sockaddr_in m_serverAddr;
    std::thread m_receiveThread;
    std::unique_ptr<std::mutex> m_mutex;
    std::unique_ptr<std::atomic<bool>> m_stopped;
    std::array<uint8_t, 32> m_cookieKey{};
    std::unordered_map<uint64_t, sockaddr_in> m_clientEndpoints;
};

} // namespace nexilis::server::af_inet

#endif
