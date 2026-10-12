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

#include <nexilis/client/protocol/nxboost/tcp_client.hh>
#include <nexilis/command_type.hh>
#include <nexilis/logger/log.hh>
#include <nexilis/ports.hh>

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>

#include <cstdint>
#include <memory>

namespace nexilis::client::nxboost
{

TCPClient::TCPClient(ClientAPI& api)
    : NxClass("client::nxboost::TCPClient"),
      NXProtocol(),
      ClientProtocol(&api),
      m_stopped(std::make_unique<std::atomic<bool>>(false)),
      m_ioContext(std::make_shared<boost::asio::io_context>()),
      m_strand(std::make_shared<boost::asio::io_context::strand>(*m_ioContext)),
      m_workGuard(std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
              boost::asio::make_work_guard(*m_ioContext))),
      m_tlsContext(createTlsContext(api)),
      m_mainSocket(std::make_shared<TlsSocket>(*m_ioContext, *m_tlsContext)),
      m_switchedSocket(std::make_shared<TlsSocket>(*m_ioContext, *m_tlsContext)),
      m_activeSocket(),
      m_resolver(*m_ioContext),
      m_sendMutex(std::make_shared<std::mutex>()),
      m_receiveMutex(std::make_shared<std::mutex>()),
      m_portSwitchingMutex(std::make_shared<std::mutex>()),
      m_mainPort(Ports::getBoostTCPPort()),
      m_switchedPort(0),
      m_useSwitchedPort(false),
      m_pendingSendsMutex(std::make_shared<std::mutex>())
{
    storeSocket(m_mainSocket);

    // Verify all shared pointers were created.
    if (!m_sendMutex || !m_receiveMutex || !m_portSwitchingMutex)
    {
        throw std::runtime_error("Failed to initialize synchronization objects");
    }
}

TCPClient::TCPClient(TCPClient&& other)
    : NxClass(std::move(other)),
      NXProtocol(std::move(other)),
      ClientProtocol(std::move(other)),
      m_ioContextThread(std::move(other.m_ioContextThread)),
      m_stopped(std::move(other.m_stopped)),
      m_ioContext(std::move(other.m_ioContext)),
      m_strand(std::move(other.m_strand)),
      m_workGuard(std::move(other.m_workGuard)),
      m_tlsContext(std::move(other.m_tlsContext)),
      m_mainSocket(std::move(other.m_mainSocket)),
      m_switchedSocket(std::move(other.m_switchedSocket)),
      m_activeSocket(other.m_activeSocket.load()),
      m_resolver(std::move(other.m_resolver)),
      m_sendMutex(std::move(other.m_sendMutex)),
      m_receiveMutex(std::move(other.m_receiveMutex)),
      m_portSwitchingMutex(std::move(other.m_portSwitchingMutex)),
      m_mainPort(std::move(other.m_mainPort)),
      m_switchedPort(std::move(other.m_switchedPort)),
      m_useSwitchedPort(other.m_useSwitchedPort.load()),
      m_pendingSends(std::move(other.m_pendingSends)),
      m_pendingSendsMutex(std::move(other.m_pendingSendsMutex))
{
    other.m_stopped.reset();
    other.m_strand.reset();
    other.m_workGuard.reset();
    other.m_sendMutex.reset();
    other.m_receiveMutex.reset();
    other.m_portSwitchingMutex.reset();
    other.m_activeSocket.store(nullptr);
}

TCPClient& TCPClient::operator=(TCPClient&& other)
{
    if (this != &other)
    {
        static_cast<NxClass&>(*this) = static_cast<NxClass&&>(other);
        static_cast<NXProtocol&>(*this) = static_cast<NXProtocol&&>(other);
        static_cast<ClientProtocol&>(*this) = static_cast<ClientProtocol&&>(other);

        if (m_ioContextThread.joinable())
        {
            m_ioContextThread.join();
        }

        m_ioContextThread = std::move(other.m_ioContextThread);
        m_stopped = std::move(other.m_stopped);
        m_ioContext = std::move(other.m_ioContext);
        m_strand = std::move(other.m_strand);
        m_workGuard = std::move(other.m_workGuard);
        m_tlsContext = std::move(other.m_tlsContext);
        m_mainSocket = std::move(other.m_mainSocket);
        m_switchedSocket = std::move(other.m_switchedSocket);
        m_activeSocket = other.m_activeSocket.load();
        m_resolver = std::move(other.m_resolver);
        m_sendMutex = std::move(other.m_sendMutex);
        m_receiveMutex = std::move(other.m_receiveMutex);
        m_portSwitchingMutex = std::move(other.m_portSwitchingMutex);
        m_mainPort = std::move(other.m_mainPort);
        m_switchedPort = std::move(other.m_switchedPort);
        m_useSwitchedPort = other.m_useSwitchedPort.load();
        m_pendingSends = std::move(other.m_pendingSends);
        m_pendingSendsMutex = std::move(other.m_pendingSendsMutex);

        other.m_stopped.reset();
        other.m_workGuard.reset();
        other.m_strand.reset();
        other.m_sendMutex.reset();
        other.m_receiveMutex.reset();
        other.m_portSwitchingMutex.reset();
    }
    return *this;
}

TCPClient::~TCPClient()
{
    stop();
    m_workGuard.reset();
}

bool TCPClient::connectToMainPort()
{
    try
    {
        Log::debug(header(), "Connect to main port: ", m_mainPort);

        auto endpoints = m_resolver.resolve(
                getClientAPI()->getBoostTCPServerAddress(),
                std::to_string(m_mainPort));

        boost::system::error_code ec;
        boost::asio::connect(m_mainSocket->lowest_layer(), endpoints, ec);

        if (ec)
        {
            Log::error(header(), "Cannot connec to main port: ", m_mainPort, " - ", ec.message());
            return false;
        }

        if (getClientAPI()->isTlsEnabled())
        {
            boost::system::error_code handshake_ec;
            m_mainSocket->handshake(boost::asio::ssl::stream_base::client, handshake_ec);
            if (handshake_ec)
            {
                Log::error(header(), "TLS handshake failed on main port: ", m_mainPort, " - ", handshake_ec.message());
                m_mainSocket->lowest_layer().close();
                return false;
            }
            Log::info(header(), "TLS-PSK established on main port: ", m_mainPort);
        }
        Log::debug(header(), "Successfully connected to main port: ", m_mainPort);
        return true;
    }
    catch (const std::exception& e)
    {
        Log::error(header(), "Exceptino connecting to main port: ", e.what());
        return false;
    }
}

bool TCPClient::connectToSwitchedPort(uint16_t port)
{
    try
    {
        Log::debug(header(), "Connecting to switched port: ", port);

        auto endpoints = m_resolver.resolve(
                getClientAPI()->getBoostTCPServerAddress(),
                std::to_string(port));

        boost::system::error_code ec;
        boost::asio::connect(m_switchedSocket->lowest_layer(), endpoints, ec);

        if (ec)
        {
            Log::error(header(), "Cannot connect to switched port: ", port, " - ", ec.message());
            return false;
        }

        if (getClientAPI()->isTlsEnabled())
        {
            boost::system::error_code handshake_ec;
            m_switchedSocket->handshake(boost::asio::ssl::stream_base::client, handshake_ec);
            if (handshake_ec)
            {
                Log::error(header(), "TLS handshake failed on switched port: ", port, " - ", handshake_ec.message());
                m_switchedSocket->lowest_layer().close();
                return false;
            }
            Log::info(header(), "TLS-PSK established on switched port: ", port);
        }

        m_switchedPort = port;
        Log::debug(header(), "Successfully connected to switched port: ", port);
        return true;
    }
    catch (const std::exception& e)
    {
        Log::error(header(), "Exception connecting to switched port: ", e.what());
        return false;
    }
}

std::shared_ptr<boost::asio::ssl::context> TCPClient::createTlsContext(ClientAPI& api)
{
    auto ctx = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tls);
    if (api.isTlsEnabled())
    {
        auto pskCtx = tls::createPskContext(api.getClientPassword(), false);
        if (!pskCtx)
        {
            Log::error("client::nxboost::TCPClient", "TLS-PSK context creation failed (is the password set?), "
                                                     "connecting without TLS");
            return ctx;
        }
        return pskCtx;
    }
    return ctx;
}

void TCPClient::stop()
{
    if (m_stopped && m_stopped->exchange(true))
    {
        return;
    }

    // Close the sockets from the strand. The io_context thread is the only
    // thread allowed to touch the ssl::stream objects; closing them from this
    // thread while an async SSL operation is in flight is a data race that
    // crashes sporadically during shutdown (intermittent SIGSEGV). Closing on
    // the strand cancels the pending async reads/writes cleanly from the io
    // thread itself.
    if (m_strand && m_ioContext)
    {
        try
        {
            boost::asio::post(*m_strand,
                              [this]()
                              {
                                  boost::system::error_code ec;
                                  for (auto* socket : {m_mainSocket.get(), m_switchedSocket.get()})
                                  {
                                      if (socket && socket->lowest_layer().is_open())
                                      {
                                          socket->lowest_layer().close(ec);
                                      }
                                  }
                              });
        }
        catch (...)
        {
        }
    }

    // Let the io thread observe the close, run its remaining handlers and exit
    // on its own. The posted close unblocks the pending read/write, so run()
    // returns once both the handlers and the work guard are gone.
    m_workGuard.reset();

    if (m_ioContextThread.joinable())
    {
        m_ioContextThread.join();
        Log::debug(header(), "io_contextThread stopped");
    }

    if (m_pendingSendsMutex)
    {
        std::lock_guard<std::mutex> lock(*m_pendingSendsMutex);
        for (auto& pair : m_pendingSends)
        {
            try
            {
                pair.second->set_exception(
                        std::make_exception_ptr(std::runtime_error("Connection closed")));
            }
            catch (...)
            {
            }
        }
        m_pendingSends.clear();
    }

    // Drain any queued (not yet initiated) writes. The io context has been
    // joined by now so the strand can no longer be running; reject their
    // promises so no future hangs.
    while (!m_writeQueue.empty())
    {
        QueuedWrite w(std::move(m_writeQueue.front()));
        m_writeQueue.pop_front();
        if (w.handler)
        {
            try
            {
                w.handler(boost::asio::error::operation_aborted, 0);
            }
            catch (...)
            {
            }
        }
    }
    m_writeInFlight = false;
}

void TCPClient::sendMessage(const nx_data& message)
{
    send(message);
}

void TCPClient::sendMessage(const nx_data& message, const std::function<void()>& callback)
{
    sendMessageWithCallback(message, callback);
}

bool TCPClient::connectToServer()
{
    if (getProtocolStatus() != ProtocolStatus::undefined)
    {
        Log::error(header(), "ProtocolStatus is not undefined when trying the first connection");
        return false;
    }

    updateProtocolStatus(ProtocolStatus::connecting);

    // First connect to main port.
    if (!connectToMainPort())
    {
        Log::error(header(), "Cannot connect to the main port in server!");
        updateProtocolStatus(ProtocolStatus::error);
        return false;
    }

    // Main socket is active for now.
    storeSocket(m_mainSocket);
    m_useSwitchedPort = false;

    updateProtocolStatus(ProtocolStatus::connected);
    return true;
}

void TCPClient::initiatePortSwitch(uint16_t port)
{
    Log::info(header(), "Initiating port switch to: ", port);

    boost::asio::post(*m_ioContext, [this, port]()
                      {
        Log::debug(header(), "Port switch task running, connecting to: ", port);
        if (connectToSwitchedPort(port))
        {
            Log::debug(header(), "Connected to switched port, updating active socket");
            std::lock_guard<std::mutex> lock(*m_portSwitchingMutex);
            storeSocket(m_switchedSocket);
            m_useSwitchedPort = true;

            if (m_mainSocket->lowest_layer().is_open())
            {
                Log::debug(header(), "Closing main socket");
                boost::system::error_code ec;
                ec = m_mainSocket->lowest_layer().cancel(ec);
                if (ec)
                {
                    Log::error(header(), "Error cancelling main socket");
                }
                ec = m_mainSocket->lowest_layer().close(ec);
                if (ec)
                {
                    Log::error(header(), "Error closing main socket");
                }
            }

            Log::info(header(), "Successfully switched to port: ", port);

            if (!m_stopped->load())
            {
                Log::debug(header(), "Starting async read on switched port");
                startAsyncRead();
            }
        }
        else
        {
            Log::error(header(), "Failed to connect to switched port: ", port);
            updateProtocolStatus(ProtocolStatus::error);
        } });
}

bool TCPClient::send(const nx_data& data)
{
    if (getProtocolStatus() != ProtocolStatus::connected)
    {
        Log::warning(header(), "Cannot send in state: ", getProtocolStatusString());
        return false;
    }

    if (m_stopped->load())
    {
        Log::warning(header(), "Cannot send when m_stopped is false");
        return false;
    }

    std::shared_ptr<TlsSocket> current_socket = loadSocket();
    if (!current_socket || !current_socket->lowest_layer().is_open())
    {
        Log::error(header(), "Socket is invalid or not open");
        return false;
    }

    // Extract message ID for this send
    uint64_t messageId = 0;
    if (data.size() >= 16)
    {
        std::memcpy(&messageId, data.data() + 8, sizeof(uint64_t));
    }

    try
    {
        auto framed = std::make_shared<nx_data>(ClientProtocol::frame(data));

        const bool tlsOn = getClientAPI()->isTlsEnabled();

        auto completionHandler = [this, current_socket, messageId, framed](const boost::system::error_code& ec,
                                                                           std::size_t size)
        {
            if (!ec)
            {
                Log::info(header(), "Message sent: ", size, " bytes, ID: ", messageId);

                // Fulfill the promise when write completes
                if (messageId != 0)
                {
                    std::lock_guard<std::mutex> lock(*m_pendingSendsMutex);
                    auto it = m_pendingSends.find(messageId);
                    if (it != m_pendingSends.end())
                    {
                        it->second->set_value();
                        m_pendingSends.erase(it);
                    }
                }
            }
            else
            {
                Log::error(header(), "Async write error: ", ec.message());

                // Fulfill with exception on error
                if (messageId != 0)
                {
                    std::lock_guard<std::mutex> lock(*m_pendingSendsMutex);
                    auto it = m_pendingSends.find(messageId);
                    if (it != m_pendingSends.end())
                    {
                        it->second->set_exception(
                                std::make_exception_ptr(
                                        std::runtime_error("Send failed: " + ec.message())));
                        m_pendingSends.erase(it);
                    }
                }
            }
        };

        // Queue the write on the strand so all ssl::stream write operations are
        // strictly serialized: only one async_write is ever in flight, and a
        // new one only starts after the previous TLS record was fully
        // flushed. Issuing overlapping async_writes on an ssl::stream lets
        // OpenSSL report "bytes written" for a record that was never queued.
        boost::asio::dispatch(*m_strand,
                              [this, tlsOn, current_socket, framed, messageId, completionHandler]()
                              {
                                  m_writeQueue.push_back(
                                          QueuedWrite{current_socket, tlsOn, framed, messageId, completionHandler});
                                  serviceWriteQueue();
                              });
        return true;
    }
    catch (const std::exception& e)
    {
        Log::error(header(), "Exception during async_write: ", e.what());

        // Clean up promise on exception
        if (messageId != 0)
        {
            std::lock_guard<std::mutex> lock(*m_pendingSendsMutex);
            auto it = m_pendingSends.find(messageId);
            if (it != m_pendingSends.end())
            {
                it->second->set_exception(std::current_exception());
                m_pendingSends.erase(it);
            }
        }
        return false;
    }
}

void TCPClient::serviceWriteQueue()
{
    // Runs only on the strand thread. Drains exactly one pending write at a
    // time so overlapping async_write operations on the ssl::stream can never
    // occur.
    while (!m_writeInFlight && !m_writeQueue.empty())
    {
        QueuedWrite w(std::move(m_writeQueue.front()));
        m_writeQueue.pop_front();

        if (m_stopped->load() || !w.socket || !w.socket->lowest_layer().is_open())
        {
            Log::warning(header(), "Socket closed before queued write could start");
            if (w.messageId != 0)
            {
                std::lock_guard<std::mutex> lock(*m_pendingSendsMutex);
                auto it = m_pendingSends.find(w.messageId);
                if (it != m_pendingSends.end())
                {
                    it->second->set_exception(
                            std::make_exception_ptr(
                                    std::runtime_error("Send failed: socket closed")));
                    m_pendingSends.erase(it);
                }
            }
            continue;
        }

        Log::debug(header(), "Initiating queued write: bytes=", w.framed->size(), " id=", w.messageId);

        m_writeInFlight = true;

        auto wrapped = [this, w](const boost::system::error_code& ec, std::size_t size)
        {
            if (w.handler)
            {
                w.handler(ec, size);
            }
            m_writeInFlight = false;
            serviceWriteQueue();
        };

        // clang-format off
        if (w.tlsOn)
        {
            boost::asio::async_write(*w.socket,
                boost::asio::buffer(*w.framed),
                boost::asio::bind_executor(*m_strand, wrapped));
        }
        else
        {
            boost::asio::async_write(w.socket->next_layer(),
                boost::asio::buffer(*w.framed),
                boost::asio::bind_executor(*m_strand, wrapped));
        }
        // clang-format on

        return;
    }
}

std::future<void> TCPClient::sendMessageAsync(const nx_data& message)
{
    // Extract message ID (second 8 bytes)
    if (message.size() < 16)
    {
        Log::error(header(), "Message too small");
        std::promise<void> emptyPromise;
        emptyPromise.set_exception(
                std::make_exception_ptr(std::runtime_error("Message too small")));
        return emptyPromise.get_future();
    }

    uint64_t messageId = 0;
    std::memcpy(&messageId, message.data() + 8, sizeof(uint64_t));

    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();

    // Store promise BEFORE calling send() so the async_write handler
    // can find it when it fires on the io_context thread.
    {
        std::lock_guard<std::mutex> lock(*m_pendingSendsMutex);
        m_pendingSends[messageId] = promise;
    }

    // Send the message (this will trigger async write)
    if (!send(message))
    {
        Log::error(header(), "Async send failed synchronously");

        // Remove the promise we just stored and reject the future.
        {
            std::lock_guard<std::mutex> lock(*m_pendingSendsMutex);
            m_pendingSends.erase(messageId);
        }
        promise->set_exception(
                std::make_exception_ptr(std::runtime_error("Send failed")));
        return future;
    }

    return future;
}

void TCPClient::start()
{
    if (connectToServer())
    {
        startAsyncRead();

        // clang-format off
        m_ioContextThread = std::thread([this]()
        {
            Log::debug(header(), "io_context thread started.");
            try
            {
                m_ioContext->run();
            }
            catch (const std::exception& e)
            {
                Log::error(header(), "io_context exception: ", e.what());
            }
            Log::debug(header(), "io_context thread stopped.");
        });
        // clang-format on

        ClientProtocol::start(getType());
    }
    else
    {
        Log::error(header(), "Failed to connect to the server");
    }
}

void TCPClient::startAsyncRead()
{
    if (m_readInProgress.exchange(true))
    {
        Log::debug(header(), "Async read already in progress, skipping");
        return;
    }

    if (m_stopped->load())
    {
        m_readInProgress = false;
        return;
    }

    auto current_socket = loadSocket();

    if (!current_socket || !current_socket->lowest_layer().is_open())
    {
        Log::warning(header(), "Socket not available for async read");
        m_readInProgress = false;
        return;
    }

    Log::debug(header(), "Starting async read on ", (m_useSwitchedPort ? "switched" : "main"), " port");
    doAsyncRead(current_socket, std::make_shared<boost::asio::streambuf>());
}

void TCPClient::doAsyncRead(std::shared_ptr<TlsSocket> current_socket,
                            std::shared_ptr<boost::asio::streambuf> receiveBuffer)
{
    if (m_stopped->load())
    {
        m_readInProgress = false;
        return;
    }

    const bool tlsOn = getClientAPI()->isTlsEnabled();
    auto readHandler = [this, receiveBuffer, current_socket](const boost::system::error_code& ec, size_t bytes_transferred)
    {
        if (ec)
        {
            m_readInProgress = false;
            handleAsyncReadError(ec);
            return;
        }

        if (m_stopped->load())
        {
            Log::debug(header(), "Read completed but client stopped");
            m_readInProgress = false;
            return;
        }

        Log::debug(header(), "Read completed: ", bytes_transferred, " bytes");

        if (bytes_transferred > 0)
        {
            // Read exactly bytes_transferred bytes (one \n-terminated message).
            // Any data beyond bytes_transferred stays in receiveBuffer for the next call.
            nx_data buffer(bytes_transferred);
            std::istream is(receiveBuffer.get());
            is.read(reinterpret_cast<char*>(buffer.data()), bytes_transferred);

            try
            {
                auto result = ClientProtocol::getClientAPI()->readMessage(buffer);
                Log::debug(header(), "readMessage result: ", ClientAPI::readResultStr(result));
                if (result == ReadResult::success)
                {
                    Log::info(header(), "Message read successfully");

                    if (!m_useSwitchedPort)
                    {
                        uint16_t switchPort = getClientAPI()->getBoostTCPServerPortNumber();
                        Log::debug(header(), "Port switch check: useSwitched=", m_useSwitchedPort.load(),
                                   " switchPort=", switchPort, " mainPort=", m_mainPort);
                        if (switchPort != 0 && switchPort != 0xFF)
                        {
                            Log::info(header(), "Port switch detected, switching from ", m_mainPort,
                                      " to ", switchPort);
                            m_readInProgress = false;
                            initiatePortSwitch(switchPort);
                            return;
                        }
                        else
                        {
                            Log::debug(header(), "No port switch needed (port=", switchPort, ")");
                        }
                    }
                }
            }
            catch (const std::exception& e)
            {
                Log::error(header(), "Error processing message: ", e.what());
            }
        }

        // Continue reading with the same buffer so leftover data is not discarded.
        if (current_socket->lowest_layer().is_open() && !m_stopped->load())
        {
            m_readInProgress = false;
            doAsyncRead(current_socket, receiveBuffer);
        }
    };

    // Initiate the async read from within the strand so SSL layer operations are
    // serialized with writes and never overlap.
    boost::asio::dispatch(*m_strand,
                          [this, tlsOn, current_socket, receiveBuffer, readHandler]()
                          {
                              if (m_stopped->load() || !current_socket->lowest_layer().is_open())
                              {
                                  m_readInProgress = false;
                                  return;
                              }

                              // clang-format off
        if (tlsOn)
        {
            boost::asio::async_read_until(*current_socket, *receiveBuffer, '\n',
                boost::asio::bind_executor(*m_strand, readHandler));
        }
        else
        {
            boost::asio::async_read_until(current_socket->next_layer(), *receiveBuffer, '\n',
                boost::asio::bind_executor(*m_strand, readHandler));
        }
                              // clang-format on
                          });
}

void TCPClient::handleAsyncReadError(const boost::system::error_code& ec)
{
    if (!ec)
    {
        return;
    }

    Log::debug(header(), "Async read error: ", ec.message(), " (", ec.value(), "), category: ", ec.category().name());

    if (ec == boost::asio::error::eof)
    {
        Log::info(header(), "Connection closed by server (EOF) - this is normal");
        updateProtocolStatus(ProtocolStatus::error);
    }
    else if (ec == boost::asio::error::connection_reset)
    {
        Log::info(header(), "Connection reset by peer");
        updateProtocolStatus(ProtocolStatus::error);
    }
    else if (ec == boost::asio::error::operation_aborted)
    {
        Log::debug(header(), "Async read aborted (normal during shutdown)");
    }
    else if (ec == boost::asio::error::not_connected)
    {
        Log::warning(header(), "Socket not connected");
        updateProtocolStatus(ProtocolStatus::error);
    }
    else
    {
        Log::error(header(), "Async read error: ", ec.message(), " (", ec.value(), ")");
        updateProtocolStatus(ProtocolStatus::error);
    }
}

std::shared_ptr<TCPClient::TlsSocket> TCPClient::loadSocket() const
{
    auto boost_ptr = m_activeSocket.load();
    if (boost_ptr)
    {
        return std::shared_ptr<TlsSocket>(boost_ptr.get(),
                                          [boost_ptr](auto&&...) mutable
                                          {
                                              boost_ptr.reset();
                                          });
    }
    return nullptr;
}

void TCPClient::storeSocket(std::shared_ptr<TlsSocket> socket)
{
    if (socket)
    {
        m_activeSocket.store(boost::shared_ptr<TlsSocket>(
                socket.get(),
                [socket](auto&&...) mutable
                {
                    socket.reset();
                }));
    }
    else
    {
        m_activeSocket.store(nullptr);
    }
}

std::shared_ptr<TCPClient::TlsSocket> TCPClient::exchangeSocket(
        std::shared_ptr<TlsSocket> new_socket)
{
    boost::shared_ptr<TlsSocket> new_boost_ptr;
    if (new_socket)
    {
        new_boost_ptr = boost::shared_ptr<TlsSocket>(
                new_socket.get(),
                [new_socket](auto&&...) mutable
                {
                    new_socket.reset();
                });
    }

    auto old_boost_ptr = m_activeSocket.exchange(new_boost_ptr);

    if (old_boost_ptr)
    {
        return std::shared_ptr<TlsSocket>(
                old_boost_ptr.get(),
                [old_boost_ptr](auto&&...) mutable
                {
                    old_boost_ptr.reset();
                });
    }

    return nullptr;
}

} // namespace nexilis::client::nxboost
