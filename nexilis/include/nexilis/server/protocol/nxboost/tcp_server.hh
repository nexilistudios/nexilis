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

#ifndef NEXILIS_BOOST_TCP_SERVER_HH
#define NEXILIS_BOOST_TCP_SERVER_HH

#include <nexilis/nx_class.hh>
#include <nexilis/ports.hh>
#include <nexilis/protocol.hh>
#include <nexilis/server/message/message_handler.hh>
#include <nexilis/server/server_config.hh>
#include <nexilis/server/server_protocol.hh>
#include <nexilis/tls.hh>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <thread>

namespace nexilis::server::nxboost
{

class TCPServer : public Protocol,
                  public ServerProtocol,
                  public NxClass
{
public:
    /// Constructor.
    explicit TCPServer(const ServerConfig& settings) noexcept;

    /// Destructor.
    ~TCPServer();

    /// Move constructor.
    TCPServer(TCPServer&& other) noexcept;

    /// Move assignment operator.
    TCPServer& operator=(TCPServer&& other) noexcept;

    /// Deleted move constructor.
    TCPServer(const TCPServer&) = delete;

    /// Deleted move assignment operator.
    TCPServer& operator=(const TCPServer&) = delete;

    /// Protocol::start() implementation.
    void start() override;

    /// Protocol::stop() implementation.
    void stop() override;

    /// Protocol::getType() implementation.
    Type getType() override
    {
        return Type::BOOST_TCP_SERVER;
    }

    /// Get the port where the server is running.
    uint16_t getPort() const;

private:
    bool sendToClient(const nx_data& data, boost::asio::ip::tcp::socket& clientSocket);
    bool startListening();
    bool acceptClients();
    void handleClient(boost::asio::ip::tcp::socket socket);
    void handleHandshake(boost::asio::ip::tcp::socket socket, std::function<void()> onCompleted);
    nx_data receiveMessage(boost::asio::ip::tcp::socket& socket);
    std::string getClientAddress(boost::asio::ip::tcp::socket& socket);
    void startSwitchedAccepting();
    uint16_t switchToSessionPort();

private:
    const uint16_t m_defaultServerPort = Ports::getBoostTCPPort();
    std::unique_ptr<std::atomic<bool>> m_firstClientConnected;
    std::unique_ptr<std::atomic<bool>> m_stopped;
    std::unique_ptr<std::mutex> m_mutex;
    std::unique_ptr<boost::asio::io_context> m_ioContext;
    std::shared_ptr<boost::asio::ssl::context> m_tlsContext;
    boost::asio::ip::tcp::acceptor m_acceptor;
    boost::asio::ip::tcp::acceptor m_switchedAcceptor;
    std::unique_ptr<std::atomic<uint16_t>> m_switchedPort;

    std::thread m_listenThread;
    std::thread m_ioContextThread;
    std::thread m_switchedAcceptThread;
    std::vector<std::thread> m_clientThreads;
};

} // namespace nexilis::server::nxboost

#endif
