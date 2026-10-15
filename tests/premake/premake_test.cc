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

#include <chrono>
#include <iostream>
#include <thread>

#include <nexilis/logger/log.hh>
#include <nexilis/ports.hh>
#include <nexilis/protocol_manager.hh>
#include <nexilis/server/client_storage.hh>

#include <nexilis/client/protocol/nxboost/tcp_client.hh>
#include <nexilis/server/protocol/nxboost/tcp_server.hh>

#include <nexilis/client/client_api.hh>
#include <nexilis/client/packet.hh>

int main()
{
    nexilis::Ports::setBoostTCPPort(55300);
    nexilis::Ports::setBoostUDPPort(55301);
    nexilis::Ports::setBoostTCPSessionPort(55302);

    nexilis::Log::startConsoleDebugging();

    nexilis::ProtocolManager protocol_manager;

    nexilis::server::ServerConfig settings;
    settings.setMode(nexilis::server::AuthenticationMode::password_protected);
    settings.setPassphrase("salasana");
    settings.setRootPassword("root");

    auto server = protocol_manager.createProtocol<nexilis::server::nxboost::TCPServer>(settings);
    server.start();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    nexilis::client::ClientConfig server_data;
    server_data.setPassword("salasana");
    server_data.setBoostTCPAddress("127.0.0.1");
    server_data.setMode(nexilis::server::AuthenticationMode::password_protected);

    nexilis::client::ClientAPI client_api(server_data);
    auto client = protocol_manager.createProtocol<nexilis::client::nxboost::TCPClient>(client_api);
    client.start();

    const auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!client.isConnected() && std::chrono::steady_clock::now() < timeout)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    bool success = true;

    if (protocol_manager.getProtocolCount() != 2)
    {
        std::cerr << "premake_test: expected 2 registered protocols, got "
                  << protocol_manager.getProtocolCount() << std::endl;
        success = false;
    }

    if (!client.isConnected())
    {
        std::cerr << "premake_test: client failed to connect" << std::endl;
        success = false;
    }

    if (!server.hasActiveConnections())
    {
        std::cerr << "premake_test: server has no active connections" << std::endl;
        success = false;
    }

    client.stop();
    server.stop();
    nexilis::server::ClientStorage::clear();
    nexilis::Log::stopLogging();

    if (!success)
    {
        return 1;
    }

    std::cout << "premake_test: ok" << std::endl;
    return 0;
}
