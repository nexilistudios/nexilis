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

#include <nexilis/boost_tcp/socket.hh>
#include <nexilis/logger/log.hh>

#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>

#include <openssl/ssl.h>

#include <cstdint>
#include <utility>

namespace nexilis::boost_tcp
{

Socket::Socket(boost::asio::io_context& io) noexcept
    : NxClass("boost_tcp::Socket-server"),
      m_context(std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tls)),
      m_stream(std::make_shared<TlsStream>(io, *m_context))
{
}

Socket::Socket(boost::asio::io_context& io, const std::string& host, uint16_t port) noexcept
    : NxClass("boost_tcp::Socket-client"),
      m_context(std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tls)),
      m_stream(std::make_shared<TlsStream>(io, *m_context)),
      m_tlsServer(false),
      m_endpointId("client:" + host + ":" + std::to_string(port))
{
}

Socket::Socket(Socket&& other) noexcept
    : NxClass(std::move(other)),
      m_context(std::move(other.m_context)),
      m_stream(std::move(other.m_stream)),
      m_connected(other.m_connected.load()),
      m_tls(other.m_tls.load()),
      m_tlsHandshaken(other.m_tlsHandshaken.load()),
      m_tlsServer(other.m_tlsServer),
      m_endpointId(std::move(other.m_endpointId))
{
    other.m_connected.store(false);
}

Socket& Socket::operator=(Socket&& other) noexcept
{
    if (this != &other)
    {
        static_cast<NxClass&>(*this) = static_cast<NxClass&&>(other);
        std::lock_guard<std::mutex> lock(m_mutex);
        std::lock_guard<std::mutex> other_lock(other.m_mutex);

        m_context = std::move(other.m_context);
        m_stream = std::move(other.m_stream);
        m_connected = other.m_connected.load();
        m_tls = other.m_tls.load();
        m_tlsHandshaken = other.m_tlsHandshaken.load();
        m_tlsServer = other.m_tlsServer;
        m_endpointId = std::move(other.m_endpointId);
        other.m_connected.store(false);
    }
    return *this;
}

bool Socket::connect(const std::string& host, uint16_t port)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_connected)
    {
        return true;
    }

    try
    {
        boost::asio::ip::tcp::resolver resolver(m_stream->lowest_layer().get_executor());
        auto endpoints = resolver.resolve(host, std::to_string(port));

        boost::asio::connect(m_stream->lowest_layer(), endpoints);

        // When TLS is enabled the stream must be rebuilt so its SSL context
        // matches the actual connection. lowest_layer() returns the socket as
        // its base type, so cast back to tcp::socket before moving.
        if (m_tls)
        {
            auto& concrete = static_cast<boost::asio::ip::tcp::socket&>(m_stream->lowest_layer());
            m_stream = std::make_shared<TlsStream>(std::move(concrete), *m_context);
        }

        m_endpointId = "client:" + host + ":" + std::to_string(port);
        m_connected = true;
        setSocketOptions();
        return true;
    }
    catch (...)
    {
        closeInternal();
        return false;
    }
}

void Socket::enableTls(std::shared_ptr<boost::asio::ssl::context> context)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (context)
    {
        m_context = std::move(context);
        m_tls = true;
    }
    else
    {
        m_tls = false;
    }
}

bool Socket::tlsEnabled() const
{
    return m_tls.load();
}

void Socket::assign(boost::asio::ip::tcp::socket&& socket)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    // Rebuild the stream around the accepted socket so a TLS context set with
    // enableTls() before assign() is picked up automatically.
    m_stream = std::make_shared<TlsStream>(std::move(socket), *m_context);
    m_tlsHandshaken.store(false);
    m_endpointId = "server:" + std::to_string(m_stream->lowest_layer().local_endpoint().port());
    m_connected = m_stream->lowest_layer().is_open();
    setSocketOptions();
}

bool Socket::ensureHandshakeInternal()
{
    if (!m_tls || m_tlsHandshaken.load())
    {
        return true;
    }

    if (!m_stream || !m_stream->lowest_layer().is_open())
    {
        m_connected = false;
        return false;
    }

    boost::system::error_code ec;
    m_stream->handshake(m_tlsServer ? boost::asio::ssl::stream_base::server
                                    : boost::asio::ssl::stream_base::client,
                        ec);
    if (ec)
    {
        Log::error(header(), "TLS handshake failed: ", ec.message());
        closeInternal();
        return false;
    }

    m_tlsHandshaken.store(true);
    return true;
}

bool Socket::send(const nx_data& data)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_connected)
    {
        return false;
    }

    if (m_tls && !ensureHandshakeInternal())
    {
        return false;
    }

    try
    {
        size_t written;
        if (m_tls)
        {
            written = boost::asio::write(*m_stream,
                                         boost::asio::buffer(data),
                                         boost::asio::transfer_all());
        }
        else
        {
            written = boost::asio::write(m_stream->next_layer(),
                                         boost::asio::buffer(data),
                                         boost::asio::transfer_all());
        }
        return written == data.size();
    }
    catch (...)
    {
        closeInternal();
        return false;
    }
}

bool Socket::receive(nx_data& data, size_t timeout_ms)
{
    // The socket mutex must NOT be held while waiting for data in select().
    // The server can call send() on this socket from another client's thread
    // (e.g. relaying a room command to this client). Holding the mutex for the
    // whole select() duration starves that thread indefinitely and deadlocks
    // the server. Check the connection state briefly, then wait for data
    // without holding the mutex.
    if (!isOpen())
    {
        return false;
    }

    // With TLS the handshake runs first, blocking on this worker thread. The
    // peer performs its half of the handshake right after the TCP connect, so
    // this completes without any application data.
    if (m_tls)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!ensureHandshakeInternal())
        {
            return false;
        }
    }

    // Plaintext data buffered inside the TLS record layer must be consumed
    // even when the kernel socket has nothing further to deliver; otherwise
    // two messages coalesced into one TLS record would stall until new
    // network traffic arrived.
    bool pending = false;
    if (m_tls)
    {
        pending = SSL_pending(m_stream->native_handle()) > 0;
    }

    if (!pending)
    {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(m_stream->lowest_layer().native_handle(), &read_fds);

        timeval timeout;
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;

        int result = select(m_stream->lowest_layer().native_handle() + 1, &read_fds, nullptr, nullptr,
                            timeout_ms > 0 ? &timeout : nullptr);

        if (result <= 0)
        {
            return false; // Timeout or error
        }
    }

    try
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_connected)
        {
            return false;
        }

        // select() reported data available, so these blocking reads complete
        // promptly and never stall the mutex for long.
        boost::system::error_code ec;

        // Read 4-byte length prefix (big-endian uint32_t)
        uint8_t len_buf[4];
        if (m_tls)
        {
            boost::asio::read(*m_stream, boost::asio::buffer(len_buf),
                              boost::asio::transfer_exactly(4), ec);
        }
        else
        {
            boost::asio::read(m_stream->next_layer(), boost::asio::buffer(len_buf),
                              boost::asio::transfer_exactly(4), ec);
        }

        if (ec)
        {
            closeInternal();
            return false;
        }

        uint32_t payload_size = (static_cast<uint32_t>(len_buf[0]) << 24) |
                                (static_cast<uint32_t>(len_buf[1]) << 16) |
                                (static_cast<uint32_t>(len_buf[2]) << 8) |
                                (static_cast<uint32_t>(len_buf[3]));

        if (payload_size == 0)
        {
            data.clear();
            return true;
        }

        // Read exactly payload_size bytes
        boost::asio::streambuf buf;
        if (m_tls)
        {
            boost::asio::read(*m_stream, buf,
                              boost::asio::transfer_exactly(payload_size), ec);
        }
        else
        {
            boost::asio::read(m_stream->next_layer(), buf,
                              boost::asio::transfer_exactly(payload_size), ec);
        }

        if (ec)
        {
            closeInternal();
            return false;
        }

        auto bufs = buf.data();
        data.assign(
                boost::asio::buffers_begin(bufs),
                boost::asio::buffers_end(bufs));
        return true;
    }
    catch (...)
    {
        closeInternal();
        return false;
    }
}

void Socket::close()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    closeInternal();
}

void Socket::closeInternal()
{
    if (!m_connected && !(m_stream && m_stream->lowest_layer().is_open()))
    {
        return;
    }

    boost::system::error_code ec;
    // Cancel ongoing async operations.
    ec = m_stream->lowest_layer().cancel(ec);
    if (ec)
    {
        Log::error(header(), "Error cancelling socket operations");
    }

    ec = m_stream->lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    if (ec)
    {
        Log::error(header(), "Error shutting down socket");
    }

    ec = m_stream->lowest_layer().close(ec);
    if (ec)
    {
        Log::error(header(), "Error closing socket");
    }
    m_connected = false;
    m_tlsHandshaken.store(false);
}

void Socket::forceClose()
{
    std::unique_lock<std::mutex> lock(m_mutex, std::try_to_lock);

    if (!lock.owns_lock())
    {
        boost::system::error_code ec;
        if (m_stream)
        {
            if (m_stream->lowest_layer().is_open())
            {
                ec = m_stream->lowest_layer().cancel(ec);
                if (ec)
                {
                    Log::error(header(), "Error cancelling socket");
                }

                ec = m_stream->lowest_layer().close(ec);
                if (ec)
                {
                    Log::error(header(), "Error closing socket");
                }
            }
        }

        if (m_connected && m_stream)
        {
            boost::system::error_code close_ec;
            close_ec = m_stream->lowest_layer().cancel(close_ec);
            if (close_ec)
            {
                Log::error(header(), "Error cancelling socket");
            }

            close_ec = m_stream->lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, close_ec);
            if (close_ec)
            {
                Log::error(header(), "Error shutting down socket");
            }

            close_ec = m_stream->lowest_layer().close(close_ec);
            if (close_ec)
            {
                Log::error(header(), "Error closing socket");
            }

            m_connected = false;
            m_tlsHandshaken.store(false);
        }
    }
}

std::string Socket::getEndpointId() const
{
    return m_endpointId;
}

bool Socket::isOpen() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stream && m_stream->lowest_layer().is_open();
}

std::string Socket::getRemoteAddress() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_stream || !m_stream->lowest_layer().is_open())
    {
        return "disconnected";
    }

    try
    {
        return m_stream->lowest_layer().remote_endpoint().address().to_string();
    }
    catch (...)
    {
        return "unknown";
    }
}

void Socket::cancelAllSocketOperations()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_connected && m_stream && m_stream->lowest_layer().is_open())
    {
        boost::system::error_code ec;
        ec = m_stream->lowest_layer().cancel(ec);
        if (ec)
        {
            Log::error(header(), "Error cancelling socket operations: ", ec.message());
        }
    }
}

void Socket::setSocketOptions()
{
    if (!m_connected)
    {
        return;
    }

    boost::system::error_code ec;
    ec = m_stream->lowest_layer().set_option(boost::asio::ip::tcp::no_delay(true), ec);
    if (ec)
    {
        Log::error(header(), "Error setting option \"no_delay\"");
    }

    ec = m_stream->lowest_layer().set_option(boost::asio::socket_base::keep_alive(true), ec);
    if (ec)
    {
        Log::error(header(), "Error setting option \"keep_alive\"");
    }

    ec = m_stream->lowest_layer().set_option(boost::asio::socket_base::linger(true, 5), ec);
    if (ec)
    {
        Log::error(header(), "Error setting option \"linger\"");
    }
}

} // namespace nexilis::boost_tcp
