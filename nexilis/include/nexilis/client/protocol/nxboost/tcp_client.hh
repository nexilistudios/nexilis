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

#ifndef NEXILIS_BOOST_TCP_CLIENT_HH
#define NEXILIS_BOOST_TCP_CLIENT_HH

#include <nexilis/client/client_protocol.hh>
#include <nexilis/nx_class.hh>
#include <nexilis/nx_protocol.hh>
#include <nexilis/tls.hh>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/smart_ptr/atomic_shared_ptr.hpp>

#include <deque>
#include <functional>
#include <thread>

namespace nexilis::client::nxboost
{

class TCPClient : public virtual NxClass,
                  public NXProtocol,
                  public ClientProtocol
{
public:
    /// TLS transport used for the TCP connections (TLS-PSK when enabled).
    using TlsSocket = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;

    /// Constructor.
    explicit TCPClient(ClientAPI& api);

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

    /// ClientProtocol::sendMessage(const nx_data&) implementation.
    void sendMessage(const nx_data& message) override;

    /// ClientProtocol::sendMessage(const nx_data&, const std::function<void()>&) implementation.
    void sendMessage(const nx_data& message, const std::function<void()>& callback) override;

    /// ClientProtocol::sendMessageAsync(const nx_data&) implementation.
    std::future<void> sendMessageAsync(const nx_data& message) override;

    /// NXProtocol::getType() implementation.
    NXProtocol::Type getType() override
    {
        return NXProtocol::Type::BOOST_TCP_CLIENT;
    }

    /// NXProtocol::start() implementation.
    void start() override;

    /// NXProtocol::stop() implementation.
    void stop() override;

protected:
    std::thread m_ioContextThread;

private:
    bool connectToServer();
    bool connectToMainPort();
    bool connectToSwitchedPort(uint16_t port);
    bool send(const nx_data& data);
    void startAsyncRead();
    void doAsyncRead(std::shared_ptr<TlsSocket> socket,
                     std::shared_ptr<boost::asio::streambuf> buffer);
    void handleAsyncReadError(const boost::system::error_code& ec);
    void initiatePortSwitch(uint16_t port);
    void serviceWriteQueue();

    /// Builds the TLS context from the ClientAPI configuration.
    static std::shared_ptr<boost::asio::ssl::context> createTlsContext(ClientAPI& api);

    // Thread-safe socket access
    std::shared_ptr<TlsSocket> loadSocket() const;
    void storeSocket(std::shared_ptr<TlsSocket> socket);
    std::shared_ptr<TlsSocket> exchangeSocket(
            std::shared_ptr<TlsSocket> socket);

private:
    std::unique_ptr<std::atomic<bool>> m_stopped;
    std::shared_ptr<boost::asio::io_context> m_ioContext;
    std::shared_ptr<boost::asio::io_context::strand> m_strand;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> m_workGuard;

    std::shared_ptr<boost::asio::ssl::context> m_tlsContext;
    std::shared_ptr<TlsSocket> m_mainSocket;
    std::shared_ptr<TlsSocket> m_switchedSocket;
    boost::atomic_shared_ptr<TlsSocket> m_activeSocket;
    boost::asio::ip::tcp::resolver m_resolver;

    std::shared_ptr<std::mutex> m_sendMutex;
    std::shared_ptr<std::mutex> m_receiveMutex;
    std::shared_ptr<std::mutex> m_portSwitchingMutex;

    uint16_t m_mainPort;
    uint16_t m_switchedPort;
    std::atomic<bool> m_useSwitchedPort;
    std::atomic<bool> m_readInProgress = false;

    struct PendingSend
    {
        uint64_t messageId = 0;
        std::shared_ptr<std::promise<void>> promise = nullptr;
    };

    std::unordered_map<uint64_t, std::shared_ptr<std::promise<void>>> m_pendingSends;
    std::shared_ptr<std::mutex> m_pendingSendsMutex;

    /// A single queued message waiting to be flushed to the transport.
    struct QueuedWrite
    {
        std::shared_ptr<TlsSocket> socket = nullptr;
        bool tlsOn = false;
        std::shared_ptr<nx_data> framed = nullptr;
        uint64_t messageId = 0;
        std::function<void(const boost::system::error_code&, std::size_t)> handler = nullptr;
    };

    /// Messages waiting to be written. Only touched from the strand.
    std::deque<QueuedWrite> m_writeQueue;
    /// True while one async_write is in flight. Only touched from the strand.
    bool m_writeInFlight = false;
};

} // namespace nexilis::client::nxboost

#endif
