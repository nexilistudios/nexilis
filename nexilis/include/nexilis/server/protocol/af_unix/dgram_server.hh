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

#ifdef __linux__

#ifndef NEXILIS_AF_UNIX_SOCK_DGRAM_SERVER_HH
#define NEXILIS_AF_UNIX_SOCK_DGRAM_SERVER_HH

#include <nexilis/nx_protocol.hh>
#include <nexilis/server/server_config.hh>
#include <nexilis/server/server_protocol.hh>

#include <sys/un.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace nexilis::server::af_unix
{

/// Server for AF_UNIX datagram sockets (SOCK_DGRAM).
/// Works similarly to the boost UDP server but over a unix-domain socket:
/// the client binds its own socket path so the server can reply to it.
class DgramServer : public NXProtocol,
                    public ServerProtocol
{
public:
    /// Constructor.
    DgramServer(const ServerConfig& settings, const std::string& socketPath);

    /// Destructor.
    ~DgramServer();

    /// Move constructor.
    DgramServer(DgramServer&& other);

    /// Move assignment operator.
    DgramServer& operator=(DgramServer&& other);

    /// Deleted copy constructor.
    DgramServer(const DgramServer& other) = delete;

    /// Deleted copy assignment.
    DgramServer& operator=(const DgramServer& other) = delete;

    /// NXProtocol::start() implementation.
    void start() override;

    /// NXProtocol::stop() implementation.
    void stop() override;

    /// NXProtocol::getType() implementation.
    NXProtocol::Type getType() override
    {
        return NXProtocol::Type::AF_UNIX_SOCK_DGRAM_SERVER;
    }

private:
    void createSocket();
    void bindSocket();
    void receiveFromClients();
    void sendToClient(const sockaddr_un& clientAddress, const nx_data& message);

private:
    std::string m_socketPath;
    int m_serverSocket = -1;
    std::thread m_receiveThread;
    std::unique_ptr<std::mutex> m_mutex;
    std::unique_ptr<std::atomic<bool>> m_stopped;
};

} // namespace nexilis::server::af_unix

#endif // NEXILIS_AF_UNIX_SOCK_DGRAM_SERVER_HH

#endif // __linux__
