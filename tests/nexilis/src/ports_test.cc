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

#include <gtest/gtest.h>

#include <nexilis/ports.hh>

using namespace nexilis;

TEST(PortsTest, BoostPortsCanBeOverridden)
{
    const auto originalTCP = Ports::getBoostTCPPort();
    const auto originalUDP = Ports::getBoostUDPPort();
    const auto originalTCPSession = Ports::getBoostTCPSessionPort();

    Ports::setBoostTCPPort(55220);
    Ports::setBoostUDPPort(55221);
    Ports::setBoostTCPSessionPort(55222);

    EXPECT_EQ(Ports::getBoostTCPPort(), 55220);
    EXPECT_EQ(Ports::getBoostUDPPort(), 55221);
    EXPECT_EQ(Ports::getBoostTCPSessionPort(), 55222);

    Ports::setBoostTCPPort(originalTCP);
    Ports::setBoostUDPPort(originalUDP);
    Ports::setBoostTCPSessionPort(originalTCPSession);
}
