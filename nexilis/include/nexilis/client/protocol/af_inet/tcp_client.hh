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

#ifndef NEXILIS_AF_INET_TCP_CLIENT_HH
#define NEXILIS_AF_INET_TCP_CLIENT_HH

#include <nexilis/client/client_api.hh>
#include <nexilis/client/client_protocol.hh>
#include <nexilis/nexilis_constants.hh>
#include <nexilis/nx_protocol.hh>

#include <netinet/in.h>

#include <future>
#include <thread>

namespace nexilis::client::af_inet
{

/// AF_INET TCP client protocol using raw sockets.
class TCPClient : public NXProtocol,
                  public ClientProtocol
{
public:
    /// Constructor.
    explicit TCPClient(ClientAPI& clientApi);

    /// Destructor.
    ~TCPClient();

    /// Move constructor.
    TCPClient(TCPClient&& other);

    /// Move assignment operator.
    TCPClient& operator=(TCPClient&& other);

    /// Deleted copy constructor.
    TCPClient(const TCPClient& other) = delete;

    /// Deleted copy assignment operator.
    TCPClient& operator=(const TCPClient& other) = delete;

    /// NXProtocol::start() implementation.
    void start() override;

    /// NXProtocol::stop() implementation.
    void stop() override;

    /// NXProtocol::getType() implementation.
    NXProtocol::Type getType() override
    {
        return NXProtocol::Type::AF_INET_TCP_CLIENT;
    }

    /// ClientProtocol::sendMessage(const nx_data&) implementation.
    void sendMessage(const nx_data& message) override;

    /// ClientProtocol::sendMessage(const nx_data&, const std::function<void()>&) implementation.
    void sendMessage(const nx_data& message, const std::function<void()>& callback) override;

    /// ClientProtocol::sendMessageAsync(const nx_data&) implementation.
    std::future<void> sendMessageAsync(const nx_data& message) override;

private:
    bool connectToServer();
    bool sendFrame(const char* data, size_t dataSize);
    ssize_t receive(char* buffer, size_t bufferSize);
    void receiveLoop();

private:
    int m_clientSocket = -1;
    sockaddr_in m_serverAddr;
    std::thread m_receiveThread;
};

} // namespace nexilis::client::af_inet

#endif
