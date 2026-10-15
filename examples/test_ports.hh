/* Copyright (C) 2026 Valtteri Viirret
   SPDX-License-Identifier: LGPL-3.0-or-later */

#ifndef NEXILIS_EXAMPLES_TEST_PORTS_HH
#define NEXILIS_EXAMPLES_TEST_PORTS_HH

#include <nexilis/ports.hh>

#include <string_view>

inline void useTestPortsIfRequested(int argc, char** argv)
{
    if (argc > 1 && std::string_view(argv[1]) == "--test-ports")
    {
        nexilis::Ports::setBoostTCPPort(55500);
        nexilis::Ports::setBoostUDPPort(55501);
        nexilis::Ports::setBoostTCPSessionPort(55502);
    }
}

#endif
