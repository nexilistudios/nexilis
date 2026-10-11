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

#include <nexilis/start_client.hh>

#include <algorithm>
#include <iterator>

#include <nexilis/client/packet.hh>

namespace nexilis
{

std::thread startClient(client::ClientAPI& client_api, TCPClient& tcp_client, std::vector<RoomInfo>& rooms, std::atomic<bool>& ready, std::mutex& mtx)
{
    // clang-format off
    return std::thread ([&client_api, &tcp_client, &rooms, &ready, &mtx]()
    {
        tcp_client.start();
        if (tcp_client.getProtocolStatus() == client::ProtocolStatus::error)
            return;

        tcp_client.sendMessage(client::Packet::Get::General::clientId(client_api));
        while (!client_api.isInitialized())
        {
            if (tcp_client.getProtocolStatus() == client::ProtocolStatus::error)
                return;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        tcp_client.sendMessage(client::Packet::Get::Info::rooms(client_api));

        std::promise<void> roomsPromise;
        auto roomsFuture = roomsPromise.get_future();
        auto waitFn = client_api.waitUntilRoomsCreated(roomsPromise);
        waitFn();
        while (roomsFuture.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready)
            if (tcp_client.getProtocolStatus() == client::ProtocolStatus::error)
                return;

        {
            std::lock_guard<std::mutex> lock(mtx);
            const auto& active_rooms = client_api.getActiveRooms();
            rooms.reserve(rooms.size() + active_rooms.size());
            std::transform(active_rooms.begin(), active_rooms.end(), std::back_inserter(rooms),
                           [](const auto& room)
                           { return RoomInfo{room.getId(), room.getName()}; });
        }
        ready = true;
    });
    // clang-format on
}

} // namespace nexilis
