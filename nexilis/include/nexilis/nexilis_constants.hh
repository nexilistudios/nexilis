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

#ifndef NEXILIS_CONSTANTS_HH
#define NEXILIS_CONSTANTS_HH

#include <cstdint>
#include <limits>

namespace nexilis
{

constexpr inline uint64_t NEXILIS_BUFFER = 1024;
constexpr inline uint64_t NEXILIS_MAX = std::numeric_limits<uint64_t>::max();
constexpr inline uint32_t NEXILIS_DEFAULT_MAX_CLIENTS = 1024;
constexpr inline uint32_t NEXILIS_DEFAULT_ROOM_CLIENT_AMOUNT = 30;
constexpr inline float NEXILIS_MAX_POSITION = 10000.0f;
constexpr inline uint64_t NEXILIS_MAX_BROADCAST_LENGTH = 1024;
constexpr inline uint64_t NEXILIS_MAX_BROADCAST_HISTORY = 100;
constexpr inline float NEXILIS_DEFAULT_MAX_BROADCAST_RATE = 5.0f;
constexpr inline uint64_t NEXILIS_DEFAULT_MAX_ROOMS = 1024;
constexpr inline uint64_t NEXILIS_DEFAULT_MAX_ROOMS_PER_CLIENT = 64;

} // namespace nexilis

#endif
