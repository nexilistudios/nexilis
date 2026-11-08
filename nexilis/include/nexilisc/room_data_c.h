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

#ifndef NEXILISC_ROOM_DATA_C_H
#define NEXILISC_ROOM_DATA_C_H

#include <nexilisc/room_context_c.h>

#include <nexilis/room_data.hh>

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    struct nexilis_RoomData
    {
        nexilis::RoomData* data;
    };

    // Returns null for a null name or allocation/random-source failure.
    nexilis_RoomData* nexilis_room_data_create(uint64_t creator_id, const char* name, nexilis_RoomContext context, uint32_t max_size);
    void nexilis_room_data_destroy(nexilis_RoomData* room_data);

    const char* nexilis_room_data_get_name(const nexilis_RoomData* room_data);
    nexilis_RoomContext nexilis_room_data_get_context(const nexilis_RoomData* room_data);
    uint32_t nexilis_room_data_get_max_size(const nexilis_RoomData* room_data);
    uint64_t nexilis_room_data_get_id(const nexilis_RoomData* room_data);
    uint64_t nexilis_room_data_get_creator_id(const nexilis_RoomData* room_data);

#ifdef __cplusplus
}
#endif

#endif
