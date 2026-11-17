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

#include <nexilis/server/protocol/af_unix/stream_server.hh>
#include <nexilis/server/runtime.hh>

#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <thread>

int main()
{
    nexilis::Log::startConsoleDebugging();
    nexilis::ProtocolManager protocol_manager;

    // Server.
    nexilis::server::ServerConfig settings;
    settings.setMode(nexilis::server::AuthenticationMode::skip);
    settings.setPassphrase("salasana");
    settings.setRootPassword("root");

    std::filesystem::create_directories("/tmp/nexilis");
    auto unix_stream_server = protocol_manager.createProtocol<nexilis::server::af_unix::StreamServer>(settings, "/tmp/nexilis/stream");

    unix_stream_server.start();

    auto condition = [](size_t)
    { return true; };
    auto f = std::function<bool()>([]()
                                   {
        std::cout << "Updating server" << std::endl;
        return true; });

    // Block the main thread and keep serving until interrupted.
    nexilis::server::runtime(condition, f, 1);
}
