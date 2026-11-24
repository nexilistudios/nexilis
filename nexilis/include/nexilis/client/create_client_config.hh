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

#ifndef NEXILIS_CLIENT_CREATE_CLIENT_CONFIG_HH
#define NEXILIS_CLIENT_CREATE_CLIENT_CONFIG_HH

#include <nexilis/client/client_config.hh>

namespace nexilis::client
{

ClientConfig createClientConfig(const std::string& ip_address, const std::string& password,
                                const std::string& accountId, const std::string& credential);

}

#endif
