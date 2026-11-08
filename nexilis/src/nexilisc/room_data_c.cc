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

#include <nexilis/room_data.hh>
#include <nexilisc/room_data_c.h>

#include <memory>

nexilis_RoomData* nexilis_room_data_create(uint64_t creator_id, const char* name, nexilis_RoomContext context, uint32_t max_size)
{
    if (!name)
        return nullptr;
    try
    {
        uint64_t room_id = nexilis::Util::getRandomUint64();
        auto data = std::make_unique<nexilis::RoomData>(creator_id, name, room_id, static_cast<nexilis::RoomData::Context>(context), max_size);
        auto wrapper = new nexilis_RoomData{data.get()};
        data.release();
        return wrapper;
    }
    catch (...)
    {
        return nullptr;
    }
}

void nexilis_room_data_destroy(nexilis_RoomData* room_data)
{
    if (room_data)
    {
        if (room_data->data)
        {
            delete room_data->data;
            room_data->data = nullptr;
        }
        delete room_data;
    }
}

const char* nexilis_room_data_get_name(const nexilis_RoomData* room_data)
{
    if (room_data && room_data->data)
    {
        std::string name = room_data->data->getName();
        char* cstr = (char*)malloc(name.size() + 1);
        if (cstr)
        {
            std::strcpy(cstr, name.c_str());
        }
        return cstr;
    }
    return nullptr;
}

nexilis_RoomContext nexilis_room_data_get_context(const nexilis_RoomData* room_data)
{
    if (room_data && room_data->data)
    {
        return static_cast<nexilis_RoomContext>(room_data->data->getContext());
    }
    return ROOM_CONTEXT_UNDEFINED;
}

uint32_t nexilis_room_data_get_max_size(const nexilis_RoomData* room_data)
{
    if (room_data && room_data->data)
    {
        return room_data->data->getMaxSize();
    }
    return 0;
}

uint64_t nexilis_room_data_get_id(const nexilis_RoomData* room_data)
{
    if (room_data && room_data->data)
    {
        return room_data->data->getId();
    }
    return 0;
}

uint64_t nexilis_room_data_get_creator_id(const nexilis_RoomData* room_data)
{
    if (room_data && room_data->data)
    {
        return room_data->data->getCreatorId();
    }
    return 0;
}
