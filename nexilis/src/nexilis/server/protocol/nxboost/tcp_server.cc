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
#include <nexilis/room_command_type.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/command/commands.hh>
#include <nexilis/server/message/auth_message.hh>
#include <nexilis/server/protocol/nxboost/tcp_server.hh>
#include <nexilis/server/room_storage.hh>
#include <nexilis/util.hh>

#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>

namespace nexilis::server::nxboost
{

TCPServer::TCPServer(const ServerConfig& settings) noexcept
    : ServerProtocol(settings),
      NxClass("server::nxboost::TCPServer"),
      m_firstClientConnected(std::make_unique<std::atomic<bool>>(false)),
      m_stopped(std::make_unique<std::atomic<bool>>(false)),
      m_mutex(std::make_unique<std::mutex>()),
      m_ioContext(std::make_unique<boost::asio::io_context>()),
      m_acceptor(*m_ioContext, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), m_defaultServerPort)),
      m_switchedAcceptor(*m_ioContext),
      m_switchedPort(std::make_unique<std::atomic<uint16_t>>(0))
{
    // Enable SO_REUSEADDR to allow port reuse.
    m_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    // m_switchedAcceptor will be configured later in switchToSessionPort().
}

TCPServer::TCPServer(TCPServer&& other) noexcept
    : NXProtocol(std::move(other)),
      ServerProtocol(std::move(other)),
      NxClass(std::move(other)),
      m_firstClientConnected(std::move(other.m_firstClientConnected) ? std::move(other.m_firstClientConnected) : std::make_unique<std::atomic<bool>>(false)),
      m_stopped(std::move(other.m_stopped) ? std::move(other.m_stopped) : std::make_unique<std::atomic<bool>>(false)),
      m_mutex(other.m_mutex ? std::move(other.m_mutex) : std::make_unique<std::mutex>()),
      m_ioContext(std::move(other.m_ioContext)),
      m_tlsContext(std::move(other.m_tlsContext)),
      m_acceptor(std::move(other.m_acceptor)),
      m_switchedAcceptor(std::move(other.m_switchedAcceptor)),
      m_switchedPort(std::move(other.m_switchedPort) ? std::move(other.m_switchedPort) : std::make_unique<std::atomic<uint16_t>>(0)),
      m_listenThread(std::move(other.m_listenThread)),
      m_ioContextThread(std::move(other.m_ioContextThread)),
      m_switchedAcceptThread(std::move(other.m_switchedAcceptThread)),
      m_clientThreads(std::move(other.m_clientThreads))
{
}

TCPServer& TCPServer::operator=(TCPServer&& other) noexcept
{
    if (this != &other)
    {
        static_cast<NXProtocol&>(*this) = static_cast<NXProtocol&&>(other);
        static_cast<ServerProtocol&>(*this) = static_cast<ServerProtocol&&>(other);
        static_cast<NxClass&>(*this) = static_cast<NxClass&&>(other);

        m_firstClientConnected = std::move(other.m_firstClientConnected);
        if (!m_firstClientConnected)
        {
            m_firstClientConnected = std::make_unique<std::atomic<bool>>(false);
        }
        m_stopped = std::move(other.m_stopped);
        if (!m_stopped)
        {
            m_stopped = std::make_unique<std::atomic<bool>>(false);
        }
        m_mutex = other.m_mutex ? std::move(other.m_mutex) : std::make_unique<std::mutex>();
        m_ioContext = std::move(other.m_ioContext);
        m_tlsContext = std::move(other.m_tlsContext);
        m_acceptor = std::move(other.m_acceptor);
        m_switchedAcceptor = std::move(other.m_switchedAcceptor);
        m_switchedPort = std::move(other.m_switchedPort);
        m_listenThread = std::move(other.m_listenThread);
        m_ioContextThread = std::move(other.m_ioContextThread);
        m_switchedAcceptThread = std::move(other.m_switchedAcceptThread);
        m_clientThreads = std::move(other.m_clientThreads);
    }
    return *this;
}

TCPServer::~TCPServer()
{
    stop();
}

void TCPServer::start()
{
    // Build the shared TLS context once. When TLS is enabled but the context
    // cannot be created (e.g. no passphrase configured), connections are
    // refused rather than silently served in plaintext.
    if (getSettings().isTlsEnabled())
    {
        m_tlsContext = tls::createPskContext(getSettings().getPassword(), true);
        if (!m_tlsContext)
        {
            Log::critical(header(), "TLS-PSK context creation failed, TLS connections will be refused "
                                    "(is the passphrase set?)");
        }
    }

    // clang-format off
    m_ioContextThread = std::thread([this]()
    {
        m_ioContext->run();
    });

    m_listenThread = std::thread([this]()
    {
        if (startListening())
        {
            acceptClients();
        }
    });
    // clang-format on
}

void TCPServer::stop()
{
    if (!m_stopped || m_stopped->exchange(true))
    {
        Log::debug(header(), "stop already in progress or completed.");
        return;
    }

    // Stop io context.
    if (m_ioContext)
    {
        Log::debug(header(), "TCPServer stopping io_context");
        m_ioContext->stop();
    }

    {
        std::lock_guard<std::mutex> lock(*m_mutex);
        for (auto& thread : m_clientThreads)
        {
            if (thread.joinable())
            {
                Log::debug(header(), "force closing client thread");
                thread.detach();
            }
        }
        m_clientThreads.clear();
    }

    auto tryJoinThread = [this](std::thread& thread, const std::string& name)
    {
        if (thread.joinable())
        {
            Log::debug(header(), "joining ", name);
            thread.join();
        }
    };

    // The accept loops poll m_stopped with a short select() timeout and exit
    // on their own. Join them before touching the acceptors: closing an
    // acceptor from this thread while the accept thread is blocked on it is a
    // data race that can crash on shutdown.
    tryJoinThread(m_switchedAcceptThread, "switchAcceptThread");
    tryJoinThread(m_listenThread, "listenThread");
    tryJoinThread(m_ioContextThread, "ioContextThread");

    // Close sockets. Only the accept thread was using the acceptors, and it
    // has been joined by now.
    boost::system::error_code ec;
    ec = m_acceptor.close(ec);
    if (ec)
    {
        Log::error(header(), "Error closing m_acceptor: ", ec.message());
    }
    ec = m_switchedAcceptor.close(ec);
    if (ec)
    {
        Log::error(header(), "Error closing m_switchedAcceptor: ", ec.message());
    }

    Log::debug(header(), "stop complete.");
}

void TCPServer::startSwitchedAccepting()
{
    // clang-format off
    m_switchedAcceptThread = std::thread([this]()
    {
        while (!m_stopped->load())
        {
            try
            {
                boost::asio::ip::tcp::socket socket(*m_ioContext);
                boost::system::error_code ec;

                if (!m_switchedAcceptor.is_open())
                {
                    if (!m_stopped->load())
                    {
                        Log::warning(header(), "Acceptor closed unexpectedly");
                    }
                    break;
                }

                // Same select-based approach for switched acceptor
                fd_set read_fds;
                FD_ZERO(&read_fds);
                FD_SET(m_switchedAcceptor.native_handle(), &read_fds);

                timeval timeout;
                timeout.tv_sec = 0;
                timeout.tv_usec = 100000; // 100ms timeout

                int result = select(m_switchedAcceptor.native_handle() + 1, &read_fds, nullptr, nullptr, &timeout);

                if (result > 0)
                {
                    ec = m_switchedAcceptor.accept(socket, ec);

                    if (ec)
                    {
                        if (ec == boost::asio::error::bad_descriptor)
                        {
                            if (!m_stopped->load())
                            {
                                Log::warning(header(), "Accept on closed acceptor");
                            }
                            break;
                        }
                        else if (ec != boost::asio::error::operation_aborted)
                        {
                            Log::error(header(), "Accept error: ", ec.message());
                        }
                        continue;
                    }

                    if (socket.is_open())
                    {
                        Log::debug(header(), "Accepted switched connection");
                        handleClient(std::move(socket));
                    }
                }
                else if (result < 0 && errno != EINTR)
                {
                    Log::error(header(), "Switched select error: ", strerror(errno));
                }
            }
            catch (...)
            {
                if (!m_stopped->load())
                {
                    Log::error(header(), "Error in switched accept");
                }
            }
        }
    });
    // clang-format on
}

bool TCPServer::startListening()
{
    m_acceptor.listen();
    Log::debug(header(), "started on default port: ", m_defaultServerPort);
    return true;
}

nx_data TCPServer::receiveMessage(boost::asio::ip::tcp::socket& socket)
{
    nx_data data;

    boost::asio::streambuf receiveBuffer;
    boost::system::error_code error_code;

    boost::asio::read(socket, receiveBuffer, boost::asio::transfer_at_least(1), error_code);

    if (error_code == boost::asio::error::eof)
    {
        return data;
    }
    else if (error_code == boost::asio::error::operation_aborted)
    {
        Log::debug(header(), "receiveMessage aborted due to shutdown");
        return data;
    }
    else if (error_code)
    {
        // Handle other errors
        Log::error(header(), "Error reading from client: ", error_code.message());
        return data;
    }

    // Get the sequence of const buffers from the streambuf.
    const boost::asio::const_buffer& receive_buffer = receiveBuffer.data();

    // Check if the buffer is empty.
    if (receive_buffer.size() == 0)
    {
        Log::info(header(), "Empty data");
        return data;
    }

    // Extract the data.
    const uint8_t* buffer_data = static_cast<const uint8_t*>(receive_buffer.data());
    size_t buffer_size = receive_buffer.size();

    // Insert the data from the buffer into the vector.
    data.insert(data.end(), buffer_data, buffer_data + buffer_size);

    return data;
}

std::string TCPServer::getClientAddress(boost::asio::ip::tcp::socket& socket)
{
    std::string clientAddress;
    try
    {
        boost::asio::ip::tcp::endpoint remoteEndpoint = socket.remote_endpoint();
        boost::asio::ip::address remoteAddress = remoteEndpoint.address();
        clientAddress = remoteAddress.to_string();
    }
    catch (const std::exception& e)
    {
        Log::error("Error getting info from remote, reason: ", e.what());
    }
    return clientAddress;
}

void TCPServer::handleHandshake(boost::asio::ip::tcp::socket socket, std::function<void()> onCompleted)
{
    ServerProtocol::connectionEstablished();
    try
    {
        auto socket_wrapper = std::make_shared<nexilis::boost_tcp::Socket>(*m_ioContext);
        if (getSettings().isTlsEnabled())
        {
            if (!m_tlsContext)
            {
                Log::error(header(), "TLS requested but no TLS context is available, refusing connection");
                return;
            }
            socket_wrapper->enableTls(m_tlsContext);
        }
        socket_wrapper->assign(std::move(socket));

        // clang-format off
        auto thread = std::thread([this, socket_wrapper, cb = std::move(onCompleted)]() mutable
        {
            try
            {
                while (!m_stopped->load() && socket_wrapper->isOpen())
                {
                    nx_data data;
                    bool got_data = socket_wrapper->receive(data);
                    if (m_stopped->load())
                    {
                        socket_wrapper->forceClose();
                        break;
                    }

                    if (!got_data)
                    {
                        if(!socket_wrapper->isOpen())
                        {
                            break;
                        }
                        continue;
                    }

                    auto clientAddress = socket_wrapper->getRemoteAddress();

                    // TODO This is the passwd message
                    auto handled_message = getMessageHandler().readMessage(clientAddress, data, &getSettings());

                    if (!handled_message)
                    {
                        continue;
                    }

                    // TODO fix this as well
                    handled_message->getUser()->setBoostTCPSend([socket_wrapper](const nx_data& bytes)
                    {
                        socket_wrapper->send(bytes);
                    });

                    auto type = handled_message->getType();

                    if (type == BaseMessage::Type::auth_message)
                    {
                        auto msgPtr = static_cast<AuthMessage*>(handled_message.get());

                        // TODO fix this also
                        CommandResult passCommand = getCommand().read(msgPtr->getData()[0], *msgPtr->getUser(), *this, msgPtr->getMessageId());

                        if (passCommand == CommandResult::success)
                        {
                            Log::info(header(), "Auth part 1 success");

                            if (!m_firstClientConnected->load())
                            {
                                Log::info(header(), "First client! Opening second port");
                                if (switchToSessionPort() == 0)
                                {
                                    Log::error(header(), "Failed to open the second port");
                                    break;
                                }
                                m_firstClientConnected->store(true);
                            }

                            uint16_t port = getPort();
                            if (port == 0)
                            {
                                Log::error(header(), "No port avainable for the client");
                                break;
                            }

                            nx_data command_data = { 0, 1, 0, 1 };
                            auto port_data = Util::convertToByteVector(getPort());
                            command_data.insert(command_data.end(), port_data.begin(), port_data.end());

                            CommandResult portCommand = getCommand().read(command_data, *msgPtr->getUser(), *this, msgPtr->getMessageId());

                            if (portCommand == CommandResult::success)
                            {
                                Log::info(header(), "Auth fully complete");
                                socket_wrapper->close();
                                cb();
                                break;
                            }
                            else
                            {
                                Log::error("Auth failed to port data");
                            }
                        }
                        else
                        {
                            Log::info(header(), "Result: ", asString(passCommand));
                        }
                    }
                    else
                    {
                        Log::critical(header(), "Wrong message type");
                    }
                }
            }
            catch (...)
            {
                Log::debug(header(), "Error with handshake");
            }

            socket_wrapper->close();
        });

        {
            std::lock_guard<std::mutex> lock(*m_mutex);
            m_clientThreads.emplace_back(std::move(thread));
        }
    }
    catch (const std::exception& e)
    {
        ServerProtocol::connectionClosed();
        Log::error(header(), "Failed to create handshake thread: ", e.what());
    }
    catch (...)
    {
        ServerProtocol::connectionClosed();
        Log::error(header(), "Other failure in handshake thread");
    }
    // clang-format on
}

void TCPServer::handleClient(boost::asio::ip::tcp::socket socket)
{
    auto client_address = getClientAddress(socket);
    auto socket_wrapper = std::make_shared<nexilis::boost_tcp::Socket>(*m_ioContext);
    if (getSettings().isTlsEnabled())
    {
        if (!m_tlsContext)
        {
            Log::error(header(), "TLS requested but no TLS context is available, refusing connection");
            return;
        }
        socket_wrapper->enableTls(m_tlsContext);
    }
    socket_wrapper->assign(std::move(socket));

    // clang-format off
    auto client_thread = std::thread([this, socket_wrapper, client_address]() mutable
    {
        if (!socket_wrapper->isOpen())
        {
            return;
        }

        uint64_t connectedUserId = 0;

        try
        {
            while (!m_stopped->load() && socket_wrapper->isOpen())
            {
                nx_data data;
                bool got_data = socket_wrapper->receive(data);

                if (m_stopped->load())
                {
                    socket_wrapper->forceClose();
                    break;
                }

                if (!got_data)
                {
                    if (!socket_wrapper->isOpen())
                    {
                        break;
                    }
                    continue;
                }

                auto handledMessage = getMessageHandler().readMessage(client_address, data, &getSettings());

                if (!handledMessage)
                {
                    Log::error(header(), "Received invalid message");
                    continue;
                }

                // Track the user ID for cleanup on disconnect.
                if (handledMessage->getUser() && connectedUserId == 0)
                {
                    connectedUserId = handledMessage->getUser()->getId();
                }

                // Always update the sending function, implement some caching later.
                handledMessage->getUser()->setBoostTCPSend([socket_wrapper](const nx_data& bytes)
                {
                    socket_wrapper->send(bytes);
                });

                /// Get the type of the message.
                auto type = handledMessage->getType();

                if (type == BaseMessage::Type::message)
                {
                    auto msgPtr = static_cast<Message*>(handledMessage.get());

                    // Everything ok with initialization.
                    CommandResult passCommand = getCommand().read(msgPtr->getData(), *msgPtr->getUser(), *this, msgPtr->getMessageId());

                    if (passCommand == CommandResult::success)
                    {
                        Log::info(header(), "command success");
                    }
                    else
                    {
                        Log::warning(header(), "Result: ", asString(passCommand));
                    }
                }
                else
                {
                    Log::critical(header(), "Wrong message type");
                }
            }
        }
        catch (const boost::system::system_error& e)
        {
            if (e.code() != boost::asio::error::operation_aborted)
            {
                Log::error(header(), "Error in client thread: ", e.what());
            }
        }
        catch (const std::exception& e)
        {
            Log::error(header(), "Exception in client thread: ", e.what());
        }

        // --- Cleanup: remove disconnected user from their room and client storage ---
        if (connectedUserId != 0)
        {
            auto* user = ClientStorage::getClientById(connectedUserId);
            if (user)
            {
                uint64_t roomId = user->getRoomId();
                if (roomId != 0)
                {
                    auto* room = RoomStorage::getRoomById(roomId);
                    if (room)
                    {
                        Log::info(header(), "Cleaning up disconnected user ", connectedUserId, " from room ", roomId);

                        // Broadcast the standard leave command to the remaining clients
                        // so they remove the disconnected user's object from their scene.
                        nx_data leaveData = {
                                static_cast<uint8_t>(CommandType::room),
                                static_cast<uint8_t>(RoomCommandType::Root::management),
                                static_cast<uint8_t>(RoomCommandType::Management::leave)};
                        DefaultArgs args(*user, *this, leaveData, 0);
                        ServerImpl::room_management_leave(args);
                    }
                }
                ClientStorage::remove(connectedUserId);
            }
        }

        if (socket_wrapper->isOpen())
        {
            socket_wrapper->close();
        }
        ServerProtocol::connectionClosed();
    });

    try
    {
        std::lock_guard<std::mutex> lock(*m_mutex);
        m_clientThreads.emplace_back(std::move(client_thread));
    }
    catch (...)
    {
        Log::critical(header(), "Failed to launch client thread");
        if (socket_wrapper->isOpen())
        {
            socket_wrapper->close();
        }
    }
    // clang-format on
}

bool TCPServer::acceptClients()
{
    try
    {
        while (!m_stopped->load())
        {
            boost::asio::ip::tcp::socket socket(*m_ioContext);
            boost::system::error_code ec;

            if (!m_acceptor.is_open())
            {
                if (!m_stopped->load())
                {
                    Log::warning(header(), "Acceptor closed suddenly");
                }
                break;
            }

            // Use select() for interruptible accept
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(m_acceptor.native_handle(), &read_fds);

            timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 100000; // 100ms timeout

            int result = select(m_acceptor.native_handle() + 1, &read_fds, nullptr, nullptr, &timeout);

            if (result > 0)
            {
                ec = m_acceptor.accept(socket, ec);
                if (ec)
                {
                    if (ec == boost::asio::error::bad_descriptor)
                    {
                        if (!m_stopped->load())
                        {
                            Log::warning(header(), "Accept on closed acceptor");
                        }
                        break;
                    }
                    else if (ec != boost::asio::error::operation_aborted)
                    {
                        Log::error(header(), "Accept error: ", ec.message());
                    }
                    continue;
                }

                if (socket.is_open())
                {
                    Log::debug(header(), "New handshake connection");
                    handleHandshake(std::move(socket), []()
                                    { Log::info("server::nxboost::TCPServer: Handshake completed!"); });
                }
            }
            else if (result < 0 && errno != EINTR)
            {
                Log::error(header(), "Select error: ", strerror(errno));
            }

            if (m_stopped->load())
            {
                break;
            }
        }
    }
    catch (...)
    {
        if (!m_stopped->load())
        {
            Log::error(header(), "Exception in accept loop");
        }
    }
    return true;
}

bool TCPServer::sendToClient(const nx_data& data, boost::asio::ip::tcp::socket& clientSocket)
{
    if (clientSocket.is_open())
    {
        boost::system::error_code ec;
        boost::asio::write(clientSocket, boost::asio::buffer(data), ec);
        if (ec)
        {
            Log::error(header(), "Failed to send data: ", ec.message());
            return false;
        }
        return true;
    }
    return false;
}

uint16_t TCPServer::switchToSessionPort()
{
    boost::system::error_code ec;

    // Close the previous switched acceptor if any.
    ec = m_switchedAcceptor.close(ec);
    if (ec)
    {
        Log::error(header(), "Failed to close previous m_switchedAcceptor");
    }

    // Configure new switched port.
    ec = m_switchedAcceptor.open(boost::asio::ip::tcp::v4(), ec);
    if (ec)
    {
        Log::error(header(), "Failed to open switched acceptor: ", ec.message());
        return 0;
    }

    m_switchedAcceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true), ec);
    if (ec)
    {
        Log::error(header(), "Failed to configure switched acceptor: ", ec.message());
        return 0;
    }

    ec = m_switchedAcceptor.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), Ports::getBoostTCPSessionPort()), ec);
    if (ec)
    {
        Log::error(header(), "Failed to bind switched acceptor: ", ec.message());
        return 0;
    }

    ec = m_switchedAcceptor.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec)
    {
        Log::error(header(), "Failed to listen on switched acceptor: ", ec.message());
        return 0;
    }

    m_switchedPort->store(m_switchedAcceptor.local_endpoint().port());

    startSwitchedAccepting();

    Log::debug(header(), "Now listening on default port: ", m_defaultServerPort, " and switched port: ", m_switchedPort->load());
    return m_switchedPort->load();
}

uint16_t TCPServer::getPort() const
{
    if (m_switchedPort && m_switchedPort.get() && m_switchedPort.get()->load())
    {
        return m_switchedPort.get()->load();
    }
    return 0;
}

} // namespace nexilis::server::nxboost
