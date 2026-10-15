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
#include <nexilis/environment.hh>
#include <nexilis/ports.hh>

int main(int argc, char** argv)
{
    nexilis::Ports::setBoostTCPPort(55200);
    nexilis::Ports::setBoostUDPPort(55201);
    nexilis::Ports::setBoostTCPSessionPort(55202);

    auto env = nexilis::envToString(nexilis::detectRuntimeType());
    std::cout << "Running tests in " << env << " environment." << std::endl;

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
