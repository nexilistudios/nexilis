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

#include <nexilis/logger/log.hh>
#include <nexilis/protocol_manager.hh>
#include <nexilis/util.hh>

#include <nexilis/client/client_api.hh>
#include <nexilis/client/packet.hh>

#include <nexilis/client/protocol/af_unix/stream_client.hh>

#include <iostream>

nexilis::client::ClientConfig getServerData(const std::string& address)
{
    nexilis::client::ClientConfig serverData;
    serverData.setPassword("salasana");
    serverData.setUnixStreamServerPath(address);
    serverData.setMode(nexilis::server::AuthenticationMode::skip);
    return serverData;
}

int main()
{
    nexilis::Log::startConsoleDebugging();
    nexilis::ProtocolManager protocol_manager;

    auto server_data = getServerData("/tmp/nexilis/stream");
    auto client_api = nexilis::client::ClientAPI(server_data);
    auto unix_stream_client = protocol_manager.createProtocol<nexilis::client::af_unix::StreamClient>(client_api);

    unix_stream_client.start();
    std::cout << "Nexilisclient start called" << std::endl;

    // Wait until the server has authenticated this client.
    client_api.waitUntilUnixStreamReady();

    unix_stream_client.stop();
}
