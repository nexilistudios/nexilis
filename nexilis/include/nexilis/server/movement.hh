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

#ifndef NEXILIS_SERVER_MOVEMENT_HH
#define NEXILIS_SERVER_MOVEMENT_HH

#include <nexilis/movement/movement_2D.hh>
#include <nexilis/movement/movement_3D.hh>
#include <nexilis/nx_protocol.hh>
#include <nexilis/server/server_config.hh>
#include <nexilis/server/user.hh>

#include <thread>

namespace nexilis::server
{

class Movement
{
public:
    static bool isInitialized();
    static void _initialize(ServerConfig& settings);

    /// Smooth movement.
    static double easing(double progress, double totalDistance);

    /// Linear movement.
    static double linear(double progress, double totalDistance);

    /// 2D movement thread.
    static std::thread object2D(std::unique_ptr<Movement2D> movement, User& user, NXProtocol& protocol);
    static std::thread object3D(std::unique_ptr<Movement3D> movement, User& user, NXProtocol& protocol);

private:
    static ServerConfig* m_settings;
};

} // namespace nexilis::server
#endif
