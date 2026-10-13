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

#include <nexilis/ports.hh>

namespace nexilis
{

uint16_t Ports::m_boostTCPPort = 54200;

uint16_t Ports::getBoostTCPPort()
{
    return m_boostTCPPort;
}

void Ports::setBoostTCPPort(uint16_t port)
{
    m_boostTCPPort = port;
}

uint16_t Ports::m_boostUDPPort = 54201;

uint16_t Ports::getBoostUDPPort()
{
    return m_boostUDPPort;
}

void Ports::setBoostUDPPort(uint16_t port)
{
    m_boostUDPPort = port;
}

uint16_t Ports::m_boostTCPSessionPort = 54202;

uint16_t Ports::getBoostTCPSessionPort()
{
    return m_boostTCPSessionPort;
}

void Ports::setBoostTCPSessionPort(uint16_t port)
{
    m_boostTCPSessionPort = port;
}

uint16_t Ports::m_inetTCPPort = 54300;

uint16_t Ports::getInetTCPPort()
{
    return m_inetTCPPort;
}

uint16_t Ports::m_inetUDPPort = 54301;

uint16_t Ports::getInetUDPPort()
{
    return m_inetUDPPort;
}

} // namespace nexilis
