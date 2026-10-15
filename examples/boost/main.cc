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

#include "../test_ports.hh"
#include <nexilis/logger/log.hh>
#include <nexilis/protocol_manager.hh>

#include <nexilis/client/protocol/nxboost/tcp_client.hh>
#include <nexilis/client/protocol/nxboost/udp_client.hh>
#include <nexilis/server/protocol/nxboost/tcp_server.hh>
#include <nexilis/server/protocol/nxboost/udp_server.hh>

#include <nexilis/client/client_api.hh>
#include <nexilis/client/packet.hh>

#include <chrono>
#include <iostream>
#include <thread>

int main(int argc, char** argv)
{
    useTestPortsIfRequested(argc, argv);
    nexilis::Log::startConsoleDebugging();

    nexilis::ProtocolManager protocol_manager;

    // Servers.
    nexilis::server::ServerConfig settings;
    settings.setMode(nexilis::server::AuthenticationMode::password_protected);
    settings.setPassphrase("salasana");
    settings.setRootPassword("root");

    auto tcp_server = protocol_manager.createProtocol<nexilis::server::nxboost::TCPServer>(settings);
    tcp_server.start();

    auto udp_server = protocol_manager.createProtocol<nexilis::server::nxboost::UDPServer>(settings);
    udp_server.start();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Clients.
    nexilis::client::ClientConfig tcp_server_data;
    tcp_server_data.setPassword("salasana");
    tcp_server_data.setBoostTCPAddress("127.0.0.1");
    tcp_server_data.setMode(nexilis::server::AuthenticationMode::password_protected);

    nexilis::client::ClientAPI tcp_client_api(tcp_server_data);
    auto tcp_client = protocol_manager.createProtocol<nexilis::client::nxboost::TCPClient>(tcp_client_api);
    tcp_client.start();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    nexilis::client::ClientConfig udp_server_data;
    udp_server_data.setPassword("salasana");
    udp_server_data.setBoostUDPAddress("127.0.0.1");
    udp_server_data.setMode(nexilis::server::AuthenticationMode::password_protected);

    nexilis::client::ClientAPI udp_client_api(udp_server_data);
    auto udp_client = protocol_manager.createProtocol<nexilis::client::nxboost::UDPClient>(udp_client_api);
    udp_client.start();

    std::cout << "Boost TCP client connected: " << (tcp_client.isConnected() ? "yes" : "no") << std::endl;
    std::cout << "Boost UDP client connected: " << (udp_client.isConnected() ? "yes" : "no") << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(1));

    udp_client.stop();
    std::cout << "Nexilis boost UDP client stopped" << std::endl;

    tcp_client.stop();
    std::cout << "Nexilis boost TCP client stopped" << std::endl;

    udp_server.stop();
    tcp_server.stop();
}
