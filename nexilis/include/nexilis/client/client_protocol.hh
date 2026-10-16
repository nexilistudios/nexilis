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

#ifndef NEXILIS_CLIENT_PROTOCOL_HH
#define NEXILIS_CLIENT_PROTOCOL_HH

#include <nexilis/client/client_api.hh>
#include <nexilis/client/protocol_status.hh>
#include <nexilis/nexilis_constants.hh>
#include <nexilis/nx_data.hh>
#include <nexilis/nx_protocol.hh>

namespace nexilis::client
{

class ClientProtocol : public virtual NxClass
{
public:
    /// Constructor.
    ClientProtocol(ClientAPI* api);

    /// Move constructor.
    ClientProtocol(ClientProtocol&& other);

    /// Move assignment operator.
    ClientProtocol& operator=(ClientProtocol&& other);

    /// Deleted copy constructor.
    ClientProtocol(const ClientProtocol& other) = delete;

    /// Deleted copy assignment operator.
    ClientProtocol& operator=(const ClientProtocol& other) = delete;

    /// Send nexilis message (nx_data) to server.
    virtual void sendMessage(const nx_data& message) = 0;

    /// Send nexilis message with callback.
    /// Call sendMessageWithCallback in the derived class.
    virtual void sendMessage(const nx_data& message, const std::function<void()>& callback) = 0;

    virtual std::future<void> sendMessageAsync(const nx_data& message) = 0;

    ClientAPI* getClientAPI()
    {
        return m_api;
    }

    /// Whether the client protocol is fully connected: the ClientAPI must be
    /// initialized and the protocol status must reflect an active connection.
    /// Transports that do not track ProtocolStatus leave it at `undefined`, in
    /// which case initialization alone is sufficient.
    bool isConnected() const
    {
        if (!m_api->isInitialized())
        {
            return false;
        }
        auto status = m_protocolStatus ? m_protocolStatus->load() : ProtocolStatus::undefined;
        return status == ProtocolStatus::connected || status == ProtocolStatus::undefined;
    }

    /// Get the connection status of client protocol.
    ProtocolStatus getProtocolStatus() const;

    /// Get string value of m_protocolStatus.
    std::string getProtocolStatusString() const;

    /// Change the connection status of client protocol.
    void updateProtocolStatus(ProtocolStatus status);

    /// Prepend a 4-byte big-endian length prefix for TCP message framing.
    static nx_data frame(const nx_data& payload);

protected:
    void start(NXProtocol::Type type);
    void sendMessageWithCallback(const nx_data& message, const std::function<void()>& callback);

private:
    /// Create a pair that contains the id of the message and the callback itself.
    std::pair<uint64_t, std::function<void()>> createCallback(const nx_data& message, const std::function<void()>& callback);

private:
    ClientAPI* m_api;
    std::unique_ptr<std::atomic<ProtocolStatus>> m_protocolStatus;
};

} // namespace nexilis::client

#endif
