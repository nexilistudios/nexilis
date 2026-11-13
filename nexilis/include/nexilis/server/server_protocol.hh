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

#ifndef NEXILIS_SERVER_PROTOCOL_HH
#define NEXILIS_SERVER_PROTOCOL_HH

#include <nexilis/logger/log.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/message/message_handler.hh>
#include <nexilis/server/server_config.hh>
#include <nexilis/server/user.hh>

#include <chrono>

namespace nexilis::server
{

class ServerProtocol
{
public:
    // Constructor.
    explicit ServerProtocol(const ServerConfig& settings);

    /// Move constructor.
    ServerProtocol(ServerProtocol&& other);

    /// Move assignment operator.
    ServerProtocol& operator=(ServerProtocol&& other);

    /// Deleted copy constructor.
    ServerProtocol(const ServerProtocol&) = delete;

    /// Deleted copy assignment operator.
    ServerProtocol& operator=(const ServerProtocol&) = delete;

    /// Get the settings for given server protocol.
    ServerConfig& getSettings()
    {
        return m_command.getServerConfig();
    }

    /// Check if there are active connections.
    bool hasActiveConnections() const
    {
        return m_activeConnections > 0;
    }

    /// Get the count of active connections.
    uint64_t activeConnectionsCount() const
    {
        return m_activeConnections;
    }

protected:
    /// Use this to parse the message before sending to Command.
    MessageHandler& getMessageHandler()
    {
        return m_messageHandler;
    }

    const MessageHandler& getMessageHandler() const
    {
        return m_messageHandler;
    }

    Command& getCommand()
    {
        return m_command;
    }

    const Command& getCommand() const
    {
        return m_command;
    }

    /// Derived class calls when a new connection is established.
    void connectionEstablished()
    {
        ++m_activeConnections;
    }

    /// Derived class calls when a connection is closed.
    void connectionClosed()
    {
        --m_activeConnections;
    }

    /// Register a stream connection after a message identifies its user.
    void registerStreamTransport(User& user, User::Transport transport);

    /// Record datagram activity and activate that transport for the user.
    void touchDatagramTransport(User& user, User::Transport transport);

    /// Finish one stream connection, removing the user when nothing remains.
    void streamTransportEnded(User& user, User::Transport transport, NXProtocol& protocol);

    /// Finish an active datagram transport, removing the user when nothing remains.
    void datagramTransportEnded(User& user, User::Transport transport, NXProtocol& protocol);

    /// Track a successful authentication that has not opened its session yet.
    void beginPendingHandshake(User& user);

    /// Consume pending presence when the session connection identifies the user.
    void completePendingHandshake(User& user);

    /// Finish a handshake that did not open a session.
    void abandonPendingHandshake(User& user, NXProtocol& protocol);

    /// Expire handshakes that never opened a session.
    void expirePendingHandshakes(NXProtocol& protocol);

    /// Cancel remaining handshakes when the protocol stops.
    void cleanupPendingHandshakes(NXProtocol& protocol);

    /// Expire datagram transports that have been idle for too long.
    void expireIdleDatagramTransports(User::Transport transport, NXProtocol& protocol);

    /// Drop every active datagram transport of the given kind on stop.
    void cleanupDatagramTransports(User::Transport transport, NXProtocol& protocol);

    /// Broadcast leave and remove a disconnected user from the registry.
    void leaveAndRemoveUser(User& user, NXProtocol& protocol);

private:
    static constexpr std::chrono::seconds pendingHandshakeTimeout{30};

    MessageHandler m_messageHandler;
    Command m_command;

    /// Active connections for the server protocol.
    std::atomic<uint64_t> m_activeConnections{0};

    /// Throttle datagram sweeps per protocol instance.
    std::chrono::steady_clock::time_point m_lastDatagramSweep{};
};

} // namespace nexilis::server

#endif
