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

#include <nexilisc/client/client_config_c.h>

nexilis_ClientConfigC* nexilis_client_config_create()
{
    auto config = new nexilis_ClientConfigC();
    auto cpp_data = new nexilis::client::ClientConfig();
    config->data = cpp_data;
    return config;
}

void nexilis_client_config_destroy(nexilis_ClientConfigC* config)
{
    if (config)
    {
        if (config->data)
        {
            delete config->data;
            config->data = nullptr;
        }
        delete config;
    }
}

void nexilis_client_config_set_password(nexilis_ClientConfigC* config, const char* password)
{
    if (config && config->data && password)
    {
        config->data->setPassword(password);
    }
}

void nexilis_client_config_set_account_credentials(nexilis_ClientConfigC* config,
                                                   const char* account_id, const char* credential)
{
    if (config && config->data && account_id && credential)
        config->data->setAccountCredentials(account_id, credential);
}

const char* nexilis_client_config_get_password(const nexilis_ClientConfigC* config)
{
    if (config && config->data)
    {
        std::string password = config->data->getPassword();
        char* cstr = (char*)malloc(password.size() + 1);
        if (cstr)
        {
            std::strcpy(cstr, password.c_str());
        }
        return cstr;
    }
    return nullptr;
}

void nexilis_client_config_set_mode(nexilis_ClientConfigC* config, int mode)
{
    if (config && config->data)
    {
        config->data->setMode(static_cast<nexilis::server::AuthenticationMode>(mode));
    }
}

void nexilis_client_config_set_message_encryption(nexilis_ClientConfigC* config, int enabled)
{
    if (config && config->data)
    {
        config->data->setMessageEncryption(enabled != 0);
    }
}

int nexilis_client_config_get_message_encryption(const nexilis_ClientConfigC* config)
{
    if (config && config->data)
    {
        return config->data->isMessageEncryptionEnabled() ? 1 : 0;
    }
    return 0;
}

void nexilis_client_config_set_tls(nexilis_ClientConfigC* config, int enabled)
{
    if (config && config->data)
    {
        config->data->setTls(enabled != 0);
    }
}

int nexilis_client_config_get_tls(const nexilis_ClientConfigC* config)
{
    if (config && config->data)
    {
        return config->data->isTlsEnabled() ? 1 : 0;
    }
    return 0;
}

void nexilis_client_config_set_server_public_key_sha256(nexilis_ClientConfigC* config, const char* hex)
{
    if (config && config->data && hex)
        config->data->setServerPublicKeySha256(hex);
}

const char* nexilis_client_config_get_server_public_key_sha256(const nexilis_ClientConfigC* config)
{
    if (!config || !config->data)
        return nullptr;
    const auto& pin = config->data->getServerPublicKeySha256();
    char* result = static_cast<char*>(malloc(pin.size() + 1));
    if (result)
        std::memcpy(result, pin.c_str(), pin.size() + 1);
    return result;
}

void nexilis_client_config_set_inet_udp(nexilis_ClientConfigC* config, const char* server_address)
{
    if (config && config->data && server_address)
    {
        config->data->setInetUDP(server_address);
    }
}

const char* nexilis_client_config_get_inet_udp_server_address(const nexilis_ClientConfigC* config)
{
    if (config && config->data)
    {
        std::string address = config->data->getInetUDPServerAddress();
        char* cstr = (char*)malloc(address.size() + 1);
        if (cstr)
        {
            std::strcpy(cstr, address.c_str());
        }
        return cstr;
    }
    return nullptr;
}

void nexilis_client_config_set_inet_tcp(nexilis_ClientConfigC* config, const char* server_address)
{
    if (config && config->data && server_address)
    {
        config->data->setInetTCP(server_address);
    }
}

const char* nexilis_client_config_get_inet_tcp_server_address(const nexilis_ClientConfigC* config)
{
    if (config && config->data)
    {
        std::string address = config->data->getInetTCPServerAddress();
        char* cstr = (char*)malloc(address.size() + 1);
        if (cstr)
        {
            std::strcpy(cstr, address.c_str());
        }
        return cstr;
    }
    return nullptr;
}

void nexilis_client_config_set_boost_tcp_address(nexilis_ClientConfigC* config, const char* server_address)
{
    if (config && config->data && server_address)
    {
        config->data->setBoostTCPAddress(server_address);
    }
}

const char* nexilis_client_config_get_boost_tcp_server_address(const nexilis_ClientConfigC* config)
{
    if (config && config->data)
    {
        std::string address = config->data->getBoostTCPServerAddress();
        char* cstr = (char*)malloc(address.size() + 1);
        if (cstr)
        {
            std::strcpy(cstr, address.c_str());
        }
        return cstr;
    }
    return nullptr;
}

void nexilis_client_config_set_boost_udp_address(nexilis_ClientConfigC* config, const char* server_address)
{
    if (config && config->data && server_address)
    {
        config->data->setBoostUDPAddress(server_address);
    }
}

const char* nexilis_client_config_get_boost_udp_server_address(const nexilis_ClientConfigC* config)
{
    if (config && config->data)
    {
        std::string address = config->data->getBoostUDPServerAddress();
        char* cstr = (char*)malloc(address.size() + 1);
        if (cstr)
        {
            std::strcpy(cstr, address.c_str());
        }
        return cstr;
    }
    return nullptr;
}

void nexilis_client_config_set_unix_dgram_server_path(nexilis_ClientConfigC* config, const char* socket_path)
{
    if (config && config->data && socket_path)
    {
        config->data->setUnixDgramServerPath(socket_path);
    }
}

const char* nexilis_client_config_get_unix_dgram_server_path(const nexilis_ClientConfigC* config)
{
    if (config && config->data)
    {
        std::string path = config->data->getUnixDgramServerPath();
        char* cstr = (char*)malloc(path.size() + 1);
        if (cstr)
        {
            std::strcpy(cstr, path.c_str());
        }
        return cstr;
    }
    return nullptr;
}

void nexilis_client_config_set_unix_stream_server_path(nexilis_ClientConfigC* config, const char* socket_path)
{
    if (config && config->data)
    {
        config->data->setUnixStreamServerPath(socket_path);
    }
}

const char* nexilis_client_config_get_unix_stream_server_path(const nexilis_ClientConfigC* config)
{
    if (config && config->data)
    {
        std::string path = config->data->getUnixStreamServerPath();
        char* cstr = (char*)malloc(path.size() + 1);
        if (cstr)
        {
            std::strcpy(cstr, path.c_str());
        }
        return cstr;
    }
    return nullptr;
}
