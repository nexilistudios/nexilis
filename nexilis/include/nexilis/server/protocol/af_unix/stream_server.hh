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

#ifndef NEXILIS_AF_UNIX_SOCK_STREAM_SERVER_HH
#define NEXILIS_AF_UNIX_SOCK_STREAM_SERVER_HH

#include <nexilis/nx_protocol.hh>
#include <nexilis/server/server_config.hh>
#include <nexilis/server/server_protocol.hh>

#include <thread>

namespace nexilis::server::af_unix
{

class StreamServer : public NXProtocol,
                     public ServerProtocol
{
public:
    /// Constructor.
    StreamServer(const ServerConfig& settings, const std::string& socketPath);

    /// Destructor.
    ~StreamServer();

    /// Move constructor.
    StreamServer(StreamServer&& other);

    /// Move assignment operator.
    StreamServer& operator=(StreamServer&& other);

    /// Deleted copy constructor.
    StreamServer(const StreamServer& other) = delete;

    /// Deleted copy assignment.
    StreamServer& operator=(const StreamServer& other) = delete;

    /// NXProtocol start() implementation.
    void start() override;

    void stop() override
    {
    }

    NXProtocol::Type getType() override
    {
        return NXProtocol::Type::AF_UNIX_SOCK_STREAM_SERVER;
    }

private:
    void createSocket();
    void bindSocket();
    void handleMessages();
    nx_data receiveMessage(int socket);
    void sendMessage(int clientSocket, const nx_data& message);

private:
    std::string m_socketPath;
    int m_serverSocket;
    nx_data m_buffer;
    std::thread m_receiveThread;
};

} // namespace nexilis::server::af_unix

#endif // NEXILIS_AF_UNIX_SOCK_STREAM_SERVER_HH

#endif // __linux__
