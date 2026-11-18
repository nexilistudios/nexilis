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

#ifndef NEXILIS_CLIENT_CONFIG_HH
#define NEXILIS_CLIENT_CONFIG_HH

#include <nexilis/auth_config.hh>

#include <cstdint>
#include <string>

namespace nexilis::client
{

class ClientConfig : public AuthConfig
{
public:
    ClientConfig() = default;

    using AuthConfig::AuthConfig;

    /// Optional stable account credentials for a server with an account
    /// authenticator. Both fields are sent only over pinned TLS Boost TCP.
    void setAccountCredentials(std::string accountId, std::string credential)
    {
        m_accountId = std::move(accountId);
        m_accountCredential = std::move(credential);
    }
    const std::string& getAccountId() const
    {
        return m_accountId;
    }
    const std::string& getAccountCredential() const
    {
        return m_accountCredential;
    }

    /// af_inet UDP
    const std::string& getInetUDPServerAddress() const
    {
        return m_inetUDPServerAddress;
    }

    void setInetUDP(const std::string& serverAddress)
    {
        m_inetUDPServerAddress = serverAddress;
    }

    uint16_t getInetUDPServerPort() const
    {
        return m_inetUDPServerPort;
    }

    void setInetUDPServerPort(uint16_t port)
    {
        m_inetUDPServerPort = port;
    }

    /// af_inet TCP
    const std::string& getInetTCPServerAddress() const
    {
        return m_inetTCPServerAddress;
    }

    void setInetTCP(const std::string& serverAddress)
    {
        m_inetTCPServerAddress = serverAddress;
    }

    uint16_t getInetTCPServerPort() const
    {
        return m_inetTCPServerPort;
    }

    void setInetTCPServerPort(uint16_t port)
    {
        m_inetTCPServerPort = port;
    }

    /// boost TCP.
    const std::string& getBoostTCPServerAddress() const
    {
        return m_boostTCPServerAddress;
    }

    void setBoostTCPAddress(const std::string& serverAddress)
    {
        m_boostTCPServerAddress = serverAddress;
    }

    uint16_t getBoostTCPServerPortNumber() const
    {
        return m_boostTCPServerPort;
    }

    void setBoostTCPPortNumber(uint16_t port)
    {
        m_boostTCPServerPort = port;
    }

    /// boost UDP.
    const std::string& getBoostUDPServerAddress() const
    {
        return m_boostUDPServerAddress;
    }

    void setBoostUDPAddress(const std::string& serverAddress)
    {
        m_boostUDPServerAddress = serverAddress;
    }

    uint16_t getBoostUDPServerPort() const
    {
        return m_boostUDPServerPort;
    }

    void setBoostUDPServerPort(uint16_t port)
    {
        m_boostUDPServerPort = port;
    }

    /// af_unix DGRAM
    const std::string& getUnixDgramServerPath() const
    {
        return m_unixDgramServerPath;
    }

    void setUnixDgramServerPath(const std::string& socketPath)
    {
        m_unixDgramServerPath = socketPath;
    }

    /// af_unix STREAM
    const std::string& getUnixStreamServerPath() const
    {
        return m_unixStreamServerPath;
    }

    void setUnixStreamServerPath(const std::string& socketPath)
    {
        m_unixStreamServerPath = socketPath;
    }

    void setProtocolPort(const std::string& protocol, uint16_t port)
    {
        if (protocol == "boost_tcp")
            m_boostTCPServerPort = port;
        else if (protocol == "boost_udp")
            m_boostUDPServerPort = port;
        else if (protocol == "inet_tcp")
            m_inetTCPServerPort = port;
        else if (protocol == "inet_udp")
            m_inetUDPServerPort = port;
    }

    /// Whether room broadcasts use a password-derived encryption key.
    /// The server knows the password and can derive the same key.
    /// \note Off by default so plaintext traffic stays plaintext unless the
    ///       developer explicitly opts in.
    bool isMessageEncryptionEnabled() const
    {
        return m_messageEncryption;
    }

    void setMessageEncryption(bool enabled)
    {
        m_messageEncryption = enabled;
    }

    /// Whether Boost TCP uses certificate-based TLS. On by default.
    bool isTlsEnabled() const
    {
        return m_tls;
    }

    void setTls(bool enabled)
    {
        m_tls = enabled;
    }

    /// Expected SHA-256 of the intended server's DER public key (SPKI), as
    /// 64 hexadecimal digits. Obtain this through a trusted channel.
    void setServerPublicKeySha256(std::string fingerprint)
    {
        m_serverPublicKeySha256 = std::move(fingerprint);
    }
    const std::string& getServerPublicKeySha256() const
    {
        return m_serverPublicKeySha256;
    }

private:
    std::string m_accountId;
    std::string m_accountCredential;
    /// af_inet UDP
    std::string m_inetUDPServerAddress;
    uint16_t m_inetUDPServerPort = 0;

    /// af_inet TCP
    std::string m_inetTCPServerAddress;
    uint16_t m_inetTCPServerPort = 0;

    /// boost TCP
    std::string m_boostTCPServerAddress;
    uint16_t m_boostTCPServerPort = 0xFF;

    /// boost UDP
    std::string m_boostUDPServerAddress;
    uint16_t m_boostUDPServerPort = 0;

    /// af_unix DGRAM
    std::string m_unixDgramServerPath;

    /// af_unix STREAM
    std::string m_unixStreamServerPath;

    /// Password-derived encryption toggle for room broadcasts.
    bool m_messageEncryption = false;

    /// TLS transport encryption toggle for TCP connections.
    bool m_tls = true;
    std::string m_serverPublicKeySha256;
};

} // namespace nexilis::client

#endif
