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

#ifndef NEXILIS_BOOST_UDP_SERVER_HH
#define NEXILIS_BOOST_UDP_SERVER_HH

#include <nexilis/nx_protocol.hh>
#include <nexilis/server/server_config.hh>
#include <nexilis/server/server_protocol.hh>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>

#include <thread>

namespace nexilis::server::nxboost
{

class UDPServer : public NXProtocol,
                  public ServerProtocol,
                  public NxClass
{
public:
    /// Constructor.
    explicit UDPServer(const ServerConfig& settings);

    /// Destructor.
    ~UDPServer();

    /// Move constructor.
    UDPServer(UDPServer&& other);

    /// Move assignment operator.
    UDPServer& operator=(UDPServer&& other);

    /// Deleted copy constructor.
    UDPServer(const UDPServer&) = delete;

    /// Deleted copy assignment operator.
    UDPServer& operator=(const UDPServer&) = delete;

    /// NXProtocol::start() implementation.
    void start() override;

    /// NXProtocol::stop() implementation.
    void stop() override;

    /// NXProtocol::getType() implementation.
    NXProtocol::Type getType() override
    {
        return NXProtocol::Type::BOOST_UDP_SERVER;
    }

    /// Get the port where the server is running.
    uint16_t getPort() const
    {
        return m_serverPort;
    }

private:
    void receiveFromClients();

private:
    std::unique_ptr<std::atomic<bool>> m_stopped;
    std::unique_ptr<boost::asio::io_context> m_ioContext;
    std::unique_ptr<std::mutex> m_mutex;
    boost::asio::ip::udp::endpoint m_remoteEndpoint;
    boost::asio::ip::udp::socket m_socket;
    nx_data m_receiveBuffer;
    std::thread m_ioContextThread;
    std::thread m_receiveThread;
    uint16_t m_serverPort;
};

} // namespace nexilis::server::nxboost

#endif
