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

#ifndef NEXILIS_BOOST_TCP_SOCKET_HH
#define NEXILIS_BOOST_TCP_SOCKET_HH

#include <nexilis/nx_class.hh>
#include <nexilis/nx_data.hh>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>

#include <memory>

namespace nexilis::boost_tcp
{

/// Thin wrapper around a boost::asio TCP connection.
///
/// When a TLS context is supplied via enableTls() the transport is upgraded
/// to TLS-PSK, typically derived from the Nexilis authentication passphrase.
/// Without it the connection stays plaintext. TLS and plaintext are mutually
/// exclusive per connection: the TLS handshake happens lazily on the first
/// I/O operation and a mismatch simply fails the connection instead of
/// silently downgrading.
class Socket : public NxClass
{
public:
    /// Server-side constructor.
    explicit Socket(boost::asio::io_context& io) noexcept;

    /// Client-side constructor.
    explicit Socket(boost::asio::io_context& io, const std::string& host, uint16_t port) noexcept;

    /// Move constructor.
    Socket(Socket&& other) noexcept;

    /// Move assigment operator.
    Socket& operator=(Socket&& other) noexcept;

    /// Connect for both the server and the client.
    bool connect(const std::string& host, uint16_t port);

    /// Server-side acceptance.
    void assign(boost::asio::ip::tcp::socket&& socket);

    /// Upgrade the connection to TLS.
    /// \param context A context from tls::createPskContext(), or nullptr to
    ///        keep the connection plaintext. Must be set before the first
    ///        send()/receive(); the handshake runs lazily there.
    void enableTls(std::shared_ptr<boost::asio::ssl::context> context);

    /// Whether TLS was enabled for this connection.
    bool tlsEnabled() const;

    bool send(const nx_data& data);
    /// Poll for the first byte, then require a complete frame within five seconds.
    /// Frames larger than NEXILIS_BUFFER and incomplete frames close the connection.
    bool receive(nx_data& data, size_t timeout_ms = 100);
    void close();
    void forceClose();

    bool isOpen() const;
    void cancelAllSocketOperations();

    std::string getRemoteAddress() const;
    std::string getEndpointId() const;

private:
    /// Performs the TLS handshake. Must be called while holding m_mutex.
    bool ensureHandshakeInternal();
    void setSocketOptions();
    void closeInternal();

private:
    using TlsStream = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;

    // Private executor lets synchronous callers drive deadlines without relying
    // on the server's shared io_context or spawning a timer thread.
    std::shared_ptr<boost::asio::io_context> m_io;
    std::shared_ptr<boost::asio::ssl::context> m_context;
    std::shared_ptr<TlsStream> m_stream;
    mutable std::mutex m_mutex;
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_tls{false};
    std::atomic<bool> m_tlsHandshaken{false};
    bool m_tlsServer = true;
    std::string m_endpointId;
};

} // namespace nexilis::boost_tcp

#endif
