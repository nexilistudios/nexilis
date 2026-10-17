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

#include <nexilis/client/client_protocol.hh>
#include <nexilis/client/packet.hh>
#include <nexilis/command_type.hh>
#include <nexilis/logger/log.hh>
#include <nexilis/message_auth.hh>
#include <nexilis/util.hh>

namespace nexilis::client
{

ClientProtocol::ClientProtocol(ClientAPI* api)
    : NxClass("ClientProtocol"),
      m_api(api),
      m_protocolStatus(std::make_unique<std::atomic<ProtocolStatus>>(ProtocolStatus::undefined))
{
}

ClientProtocol::ClientProtocol(ClientProtocol&& other)
    : NxClass(std::move(other)),
      m_api(other.m_api),
      m_protocolStatus(std::move(other.m_protocolStatus))
{
    if (!m_protocolStatus)
    {
        m_protocolStatus = std::make_unique<std::atomic<ProtocolStatus>>(ProtocolStatus::undefined);
    }
    other.m_api = nullptr;
    other.m_protocolStatus.reset();
}

ClientProtocol& ClientProtocol::operator=(ClientProtocol&& other)
{
    if (this != &other)
    {
        m_api = other.m_api;
        other.m_api = nullptr;

        m_protocolStatus = std::move(other.m_protocolStatus);
        if (!m_protocolStatus)
        {
            m_protocolStatus = std::make_unique<std::atomic<ProtocolStatus>>(ProtocolStatus::undefined);
        }
        other.m_protocolStatus.reset();
        NxClass::operator=(std::move(other));
    }
    return *this;
}

ProtocolStatus ClientProtocol::getProtocolStatus() const
{
    return m_protocolStatus.get()->load();
}

std::string ClientProtocol::getProtocolStatusString() const
{
    if (!m_protocolStatus.get())
    {
        return "";
    }

    return ProtocolUtils::getProtocolStatusAsString(m_protocolStatus.get()->load());
}

void ClientProtocol::updateProtocolStatus(ProtocolStatus status)
{
    if (m_protocolStatus)
    {
        m_protocolStatus->store(status);
    }
}

nx_data ClientProtocol::frame(const nx_data& payload)
{
    nx_data framed;
    uint32_t net_len = static_cast<uint32_t>(payload.size());
    framed.push_back(static_cast<uint8_t>((net_len >> 24) & 0xFF));
    framed.push_back(static_cast<uint8_t>((net_len >> 16) & 0xFF));
    framed.push_back(static_cast<uint8_t>((net_len >> 8) & 0xFF));
    framed.push_back(static_cast<uint8_t>(net_len & 0xFF));
    framed.insert(framed.end(), payload.begin(), payload.end());
    return framed;
}

nx_data ClientProtocol::authenticate(const nx_data& message) const
{
    if (!m_api->isInitialized())
        return message;
    return message_auth::sign(message, m_api->getUdpToken());
}

void ClientProtocol::sendMessageWithCallback(const nx_data& message, const std::function<void()>& callback)
{
    m_api->addCallback(ClientProtocol::createCallback(message, callback));
    sendMessage(message);
}

std::pair<uint64_t, std::function<void()>> ClientProtocol::createCallback(const nx_data& message, const std::function<void()>& callback)
{
    // Vector without client id (8 bytes).
    auto messageWithoutClientId = Util::removeAmountOfBytesFromVector(message, 8);

    // Next eight bytes is the message id.
    uint64_t messageId = Util::uint64FromFront(messageWithoutClientId);

    return std::make_pair(messageId, callback);
}

void ClientProtocol::start(NXProtocol::Type type)
{
    if (m_api->isInitialized())
    {
        Log::debug(header(), "Password already sent using another protocol, waiting for ready");
        switch (type)
        {
            case NXProtocol::Type::BOOST_TCP_CLIENT:
                getClientAPI()->waitUntilBoostTCPReady();
                break;
            case NXProtocol::Type::BOOST_UDP_CLIENT:
                getClientAPI()->waitUntilBoostUDPReady();
                break;
            case NXProtocol::Type::AF_INET_TCP_CLIENT:
                getClientAPI()->waitUntilInetTCPReady();
                break;
            case NXProtocol::Type::AF_INET_UDP_CLIENT:
                getClientAPI()->waitUntilInetUDPReady();
                break;
            case NXProtocol::Type::AF_UNIX_SOCK_STREAM_CLIENT:
                getClientAPI()->waitUntilUnixStreamReady();
                break;
            case NXProtocol::Type::AF_UNIX_SOCK_DGRAM_CLIENT:
                getClientAPI()->waitUntilUnixDgramReady();
                break;
            default:
                break;
        }
        return;
    }

    switch (m_api->getMode())
    {
        case server::AuthenticationMode::password_protected:
        {
            auto password = m_api->getClientPassword();
            Log::debug(header(), "Authenticating with server password");
            auto message = Util::convertToByteVector(password.c_str(), password.size());
            sendMessage(message);
            break;
        }
        case server::AuthenticationMode::skip:
        {
            // The client does not know its id yet, so perform a "getting
            // client_id" handshake with a zero client id prefix. The server
            // replies with the assigned client id which initializes the client.
            Log::debug(header(), "Authenticating by skipping authentication");
            nx_data message;
            _Packet::emplaceAll(message, static_cast<uint64_t>(0));          // client id
            _Packet::emplaceAll(message, getClientAPI()->getNewMessageId()); // message id
            message.emplace_back(static_cast<uint8_t>(CommandType::getting));
            message.emplace_back(0);
            message.emplace_back(0);
            sendMessage(message);
            break;
        }
        default:
        {
            Log::error("Unhandled authentication mode: ", static_cast<int>(m_api->getMode()));
            break;
        }
    }

    Log::debug(header(), "Client protocol type: ", NXProtocol::typeToString(type));
    switch (type)
    {
        case NXProtocol::Type::BOOST_TCP_CLIENT:
            Log::debug(header(), "Waiting for Boost TCP to be ready");
            getClientAPI()->waitUntilBoostTCPReady();
            Log::debug(header(), "Boost TCP is ready");
            break;
        case NXProtocol::Type::BOOST_UDP_CLIENT:
            Log::debug(header(), "Waiting for Boost UDP to be ready");
            getClientAPI()->waitUntilBoostUDPReady();
            Log::debug(header(), "Boost UDP is ready");
            break;
        case NXProtocol::Type::AF_INET_TCP_CLIENT:
            getClientAPI()->waitUntilInetTCPReady();
            break;
        case NXProtocol::Type::AF_INET_UDP_CLIENT:
            getClientAPI()->waitUntilInetUDPReady();
            break;
        case NXProtocol::Type::AF_UNIX_SOCK_STREAM_CLIENT:
            getClientAPI()->waitUntilUnixStreamReady();
            break;
        case NXProtocol::Type::AF_UNIX_SOCK_DGRAM_CLIENT:
            getClientAPI()->waitUntilUnixDgramReady();
            break;

        case NXProtocol::Type::AF_INET_TCP_SERVER:
        case NXProtocol::Type::AF_INET_UDP_SERVER:
        case NXProtocol::Type::AF_UNIX_SOCK_DGRAM_SERVER:
        case NXProtocol::Type::AF_UNIX_SOCK_STREAM_SERVER:
        case NXProtocol::Type::BOOST_TCP_SERVER:
        case NXProtocol::Type::BOOST_UDP_SERVER:
            Log::error(header(), "This function cannot be called via server protocol");
            return;

        case NXProtocol::Type::UNKNOWN:
            Log::error(header(), "This function called via unknown protocol");
            return;
    }
}

} // namespace nexilis::client
