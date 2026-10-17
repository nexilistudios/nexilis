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

#ifndef NEXILIS_AF_UNIX_SOCK_STREAM_CLIENT_HH
#define NEXILIS_AF_UNIX_SOCK_STREAM_CLIENT_HH

#include <nexilis/client/client_api.hh>
#include <nexilis/client/client_protocol.hh>
#include <nexilis/nexilis_constants.hh>
#include <nexilis/nx_protocol.hh>

#include <sys/un.h>

#include <atomic>
#include <future>
#include <mutex>
#include <thread>

namespace nexilis::client::af_unix
{

class StreamClient : public virtual NxClass,
                     public NXProtocol,
                     public ClientProtocol
{
public:
    /// Constructor.
    explicit StreamClient(ClientAPI& clientApi);

    /// Destructor.
    ~StreamClient();

    /// Move constructor.
    StreamClient(StreamClient&& other);

    /// Move assignment operator.
    StreamClient& operator=(StreamClient&& other);

    /// Deleted copy constructor.
    StreamClient(const StreamClient& other) = delete;

    /// Deleted copy assignment operator.
    StreamClient& operator=(const StreamClient& other) = delete;

    /// NXProtocol::start() implementation.
    void start() override;

    /// NXProtocol::stop() implementation.
    void stop() override;

    /// NXProtocol::getType() implementation.
    NXProtocol::Type getType() override
    {
        return NXProtocol::Type::AF_UNIX_SOCK_STREAM_CLIENT;
    }

    /// ClientProtocol::sendMessage(const nx_data&) implementation.
    void sendMessage(const nx_data& message) override;

    /// ClientProtocol::sendMessage(const nx_data&, const std::function<void()>&) implementation.
    void sendMessage(const nx_data& message, const std::function<void()>& callback) override;

    /// ClientProtocol::sendMessageAsync(const nx_data&) implementation.
    std::future<void> sendMessageAsync(const nx_data& message) override;

private:
    // Initialize sockets and stuff.
    void createSocket();
    void connectToServer();

    /// Internal function for sending messages to the server.
    void sendMsg(const nx_data& message);

    /// Receive messages from the server.
    nx_data receiveMessage();

private:
    std::string m_serverSocketPath;
    int m_clientSocket;
    sockaddr_un m_serverAddr;
    std::thread m_receiveThread;
    std::unique_ptr<std::mutex> m_mutex;
    std::unique_ptr<std::atomic<bool>> m_running;
};

} // namespace nexilis::client::af_unix

#endif
#endif
