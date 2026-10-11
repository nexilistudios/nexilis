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

#ifndef NEXILIS_PORTS_HH
#define NEXILIS_PORTS_HH

#include <cstdint>

namespace nexilis
{

/// Default listening ports for the network transports.
class Ports
{
public:
    static uint16_t getBoostTCPPort();

    static uint16_t getBoostUDPPort();

    static uint16_t getBoostTCPSessionPort();

    static uint16_t getInetTCPPort();

    static uint16_t getInetUDPPort();

private:
    static uint16_t m_boostTCPPort;
    static uint16_t m_boostUDPPort;
    static uint16_t m_boostTCPSessionPort;
    static uint16_t m_inetTCPPort;
    static uint16_t m_inetUDPPort;
};

} // namespace nexilis

#endif
