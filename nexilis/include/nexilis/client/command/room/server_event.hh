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

#ifndef NEXILIS_CLIENT_COMMAND_ROOM_SERVER_EVENT_HH
#define NEXILIS_CLIENT_COMMAND_ROOM_SERVER_EVENT_HH

#include <nexilis/client/base_api_command.hh>
#include <nexilis/client/client_api.hh>

namespace nexilis::client
{
class RoomServerEventCommand : public BaseAPICommand
{
public:
    explicit RoomServerEventCommand(ClientAPI::ServerRoomEvent event)
        : m_event(std::move(event))
    {
    }

    ReadResult execute(ClientAPI&, ClientAPI::ClientAPIData& data) override
    {
        data.pushServerRoomEvent(std::move(m_event));
        return ReadResult::success;
    }

private:
    ClientAPI::ServerRoomEvent m_event;
};
} // namespace nexilis::client

#endif
