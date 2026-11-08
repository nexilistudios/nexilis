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

#ifndef NEXILIS_CLIENT_COMMAND_SET_USERNAME_HH
#define NEXILIS_CLIENT_COMMAND_SET_USERNAME_HH

#include <nexilis/client/base_api_command.hh>

namespace nexilis::client
{

class SetUsernameCommand : public BaseAPICommand
{
public:
    SetUsernameCommand(uint64_t client_id, std::string username)
        : m_client_id(client_id), m_username(std::move(username))
    {
    }

    ReadResult execute(ClientAPI& api, ClientAPI::ClientAPIData& data) override
    {
        const uint64_t id = m_client_id == 0 ? api.getClientId() : m_client_id;
        std::lock_guard lock(*data.getRoomsMutex());
        for (auto& room : data.getCurrentlyActiveRooms())
            for (auto& client : room.getClients())
                if (client.getId() == id)
                    client.setUsername(m_username);
        return ReadResult::success;
    }

private:
    uint64_t m_client_id;
    std::string m_username;
};

} // namespace nexilis::client

#endif
