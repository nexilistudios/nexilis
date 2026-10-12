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

#ifndef NEXILIS_BOOST_UDP_CLIENT_HH
#define NEXILIS_BOOST_UDP_CLIENT_HH

#include <nexilis/client/client_protocol.hh>
#include <nexilis/nx_protocol.hh>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>

#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace nexilis::client::nxboost
{

class UDPClient : public virtual NxClass,
                  public NXProtocol,
                  public ClientProtocol
{
public:
    /// Constructor.
    explicit UDPClient(ClientAPI& api);

    /// Destructor.
    ~UDPClient();

    /// Move constructor.
    UDPClient(UDPClient&& other);

    /// Move assignment operator.
    UDPClient& operator=(UDPClient&& other);

    /// Deleted copy constructor.
    UDPClient(const UDPClient&) = delete;

    /// Deleted copy assignment operator.
    UDPClient& operator=(const UDPClient&) = delete;

    /// NXProtocol::start() implementation.
    void start() override;

    /// NXProtocol::stop() implementation.
    void stop() override;

    /// NXProtocol::getType() implementation.
    NXProtocol::Type getType() override
    {
        return NXProtocol::Type::BOOST_UDP_CLIENT;
    }

    /// ClientProtocol::sendMessage(const nx_data& message) implementation.
    void sendMessage(const nx_data& message) override;

    /// ClientProtocol::sendMessage(const nx_data&, const std::function<void()>&) implementation.
    void sendMessage(const nx_data& message, const std::function<void()>& callback) override;

    /// ClientProtocol::sendMessageAsync(const nx_data&) implementation.
    std::future<void> sendMessageAsync(const nx_data& message) override;

private:
    void receiveLoop();

private:
    std::unique_ptr<std::atomic<bool>> m_stopped;
    std::unique_ptr<boost::asio::io_context> m_ioContext;
    std::thread m_ioContextThread;
    std::thread m_receiveMessageThread;
    std::unique_ptr<std::mutex> m_mutex;
    boost::asio::ip::udp::socket m_socket;
    std::vector<char> m_receiveBuffer;
    boost::asio::ip::udp::endpoint m_remoteEndpoint;
    boost::asio::ip::udp::endpoint m_sendEndpoint;

    std::unordered_map<uint64_t, std::shared_ptr<std::promise<void>>> m_pendingSends;
    std::shared_ptr<std::mutex> m_pendingSendsMutex;
};

} // namespace nexilis::client::nxboost

#endif
