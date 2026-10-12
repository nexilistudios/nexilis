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

#ifndef NEXILIS_AF_UNIX_SOCK_DGRAM_CLIENT_HH
#define NEXILIS_AF_UNIX_SOCK_DGRAM_CLIENT_HH

#include <nexilis/client/client_api.hh>
#include <nexilis/client/client_protocol.hh>
#include <nexilis/nexilis_constants.hh>
#include <nexilis/nx_protocol.hh>

#include <sys/un.h>

#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace nexilis::client::af_unix
{

/// Client for AF_UNIX datagram sockets (SOCK_DGRAM).
/// Works similarly to the boost UDP client but over a unix-domain socket:
/// the client binds its own socket path so the server can reply to it.
class DgramClient : public virtual NxClass,
                    public NXProtocol,
                    public ClientProtocol
{
public:
    /// Constructor.
    explicit DgramClient(ClientAPI& clientApi);

    /// Destructor.
    ~DgramClient();

    /// Move constructor.
    DgramClient(DgramClient&& other);

    /// Move assignment operator.
    DgramClient& operator=(DgramClient&& other);

    /// Deleted copy constructor.
    DgramClient(const DgramClient& other) = delete;

    /// Deleted copy assignment operator.
    DgramClient& operator=(const DgramClient& other) = delete;

    /// NXProtocol::start() implementation.
    void start() override;

    /// NXProtocol::stop() implementation.
    void stop() override;

    /// NXProtocol::getType() implementation.
    NXProtocol::Type getType() override
    {
        return NXProtocol::Type::AF_UNIX_SOCK_DGRAM_CLIENT;
    }

    /// ClientProtocol::sendMessage(const nx_data&) implementation.
    void sendMessage(const nx_data& message) override;

    /// ClientProtocol::sendMessage(const nx_data&, const std::function<void()>&) implementation.
    void sendMessage(const nx_data& message, const std::function<void()>& callback) override;

    /// ClientProtocol::sendMessageAsync(const nx_data&) implementation.
    std::future<void> sendMessageAsync(const nx_data& message) override;

private:
    void createSocket();
    void receiveLoop();

private:
    std::string m_serverSocketPath;
    std::string m_clientSocketPath;
    int m_clientSocket = -1;
    sockaddr_un m_serverAddr{};
    std::thread m_receiveThread;
    std::unique_ptr<std::mutex> m_mutex;
    std::unique_ptr<std::atomic<bool>> m_stopped;
};

} // namespace nexilis::client::af_unix

#endif // NEXILIS_AF_UNIX_SOCK_DGRAM_CLIENT_HH

#endif // __linux__
