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

#ifndef NEXILIS_AF_INET_UDP_CLIENT_HH
#define NEXILIS_AF_INET_UDP_CLIENT_HH

#include <nexilis/client/client_api.hh>
#include <nexilis/client/client_protocol.hh>
#include <nexilis/nexilis_constants.hh>
#include <nexilis/nx_protocol.hh>

#include <netinet/in.h>

#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <thread>

namespace nexilis::client::af_inet
{

/// AF_INET UDP client protocol using raw sockets.
/// Works similarly to the boost UDP client but over a plain UDP socket.
class UDPClient : public virtual NxClass,
                  public NXProtocol,
                  public ClientProtocol
{
public:
    /// Constructor.
    explicit UDPClient(ClientAPI& clientApi);

    /// Destructor.
    ~UDPClient();

    /// Move constructor.
    UDPClient(UDPClient&& other);

    /// Move assignment operator.
    UDPClient& operator=(UDPClient&& other);

    /// Deleted copy constructor.
    UDPClient(const UDPClient& other) = delete;

    /// Deleted copy assignment operator.
    UDPClient& operator=(const UDPClient& other) = delete;

    /// NXProtocol::start() implementation.
    void start() override;

    /// NXProtocol::stop() implementation.
    void stop() override;

    /// NXProtocol::getType() implementation.
    NXProtocol::Type getType() override
    {
        return NXProtocol::Type::AF_INET_UDP_CLIENT;
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
    bool sendData(const char* data, size_t dataSize);

private:
    int m_clientSocket = -1;
    sockaddr_in m_serverAddr;
    std::thread m_receiveThread;
    std::unique_ptr<std::mutex> m_mutex;
    std::unique_ptr<std::atomic<bool>> m_stopped;
    nx_data m_pendingAuth;
};

} // namespace nexilis::client::af_inet

#endif
