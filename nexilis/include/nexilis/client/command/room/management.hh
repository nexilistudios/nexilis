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

#ifndef NEXILIS_CLIENT_COMMAND_ROOM_MANAGEMENT_HH
#define NEXILIS_CLIENT_COMMAND_ROOM_MANAGEMENT_HH

#include <nexilis/client/base_api_command.hh>
#include <nexilis/client/client_api.hh>

namespace nexilis::client
{

class RoomManagementCommand : public BaseAPICommand
{
public:
    RoomManagementCommand(std::string action, uint64_t room_id, uint64_t client_id,
                          std::string room_name = "", uint64_t room_context = 0,
                          std::string username = "", bool overlap_allowed = true,
                          uint32_t max_size = NEXILIS_DEFAULT_ROOM_CLIENT_AMOUNT)
        : m_action(action), m_room_id(room_id), m_client_id(client_id),
          m_room_name(std::move(room_name)), m_room_context(room_context),
          m_username(std::move(username)), m_overlap_allowed(overlap_allowed),
          m_max_size(max_size)
    {
    }

    ReadResult execute(ClientAPI& api, ClientAPI::ClientAPIData& data) override
    {
        auto& mtx = data.getRoomsMutex();
        std::lock_guard<std::mutex> lock(*mtx);

        auto& rooms = data.getCurrentlyActiveRooms();

        if (m_action == "join")
        {
            for (auto& room : rooms)
            {
                if (room.getId() == m_room_id)
                {
                    for (auto& client : room.getClients())
                    {
                        if (client.getId() == m_client_id)
                        {
                            if (!m_username.empty())
                            {
                                client.setUsername(m_username);
                            }
                            return ReadResult::success;
                        }
                    }

                    ClientSession session(m_client_id, &api);
                    session.setUsername(m_username);
                    room.addClient(std::move(session));
                    return ReadResult::success;
                }
            }
            return ReadResult::command_execution;
        }
        else if (m_action == "leave")
        {
            for (auto& room : rooms)
            {
                if (room.getId() == m_room_id)
                {
                    room.removeClient(m_client_id);
                    return ReadResult::success;
                }
            }
            return ReadResult::command_execution;
        }
        else if (m_action == "create")
        {
            auto roomData = RoomData(m_client_id, m_room_name, m_room_id,
                                     static_cast<RoomData::Context>(m_room_context), m_max_size);
            rooms.emplace_back(Room(roomData, std::vector<ClientSession>()));
            return ReadResult::success;
        }
        else if (m_action == "set_overlap")
        {
            api.setOverLapStatus2D(m_overlap_allowed);
            return ReadResult::success;
        }
        return ReadResult::not_found;
    }

private:
    std::string m_action;
    uint64_t m_room_id;
    uint64_t m_client_id;
    std::string m_room_name;
    uint64_t m_room_context;
    std::string m_username;
    bool m_overlap_allowed;
    uint32_t m_max_size;
};

} // namespace nexilis::client

#endif
