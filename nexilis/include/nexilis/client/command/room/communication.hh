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

#ifndef NEXILIS_CLIENT_COMMAND_ROOM_COMMUNICATION_HH
#define NEXILIS_CLIENT_COMMAND_ROOM_COMMUNICATION_HH

#include <nexilis/client/base_api_command.hh>
#include <nexilis/client/client_api.hh>
#include <nexilis/crypto.hh>
#include <nexilis/logger/log.hh>

namespace nexilis::client
{

class RoomCommunicationCommand : public BaseAPICommand
{
public:
    RoomCommunicationCommand(std::string action, uint64_t room_id, uint64_t client_id,
                             const std::string& message, bool replayed = false)
        : m_action(action), m_room_id(room_id), m_client_id(client_id),
          m_message(message), m_replayed(replayed)
    {
    }

    ReadResult execute(ClientAPI& api, ClientAPI::ClientAPIData& data) override
    {
        auto& mtx = data.getRoomsMutex();
        std::lock_guard<std::mutex> lock(*mtx);

        auto& rooms = data.getCurrentlyActiveRooms();

        if (m_action == "broadcast" || m_action == "broadcast_encrypted")
        {
            // Encrypted messages are relayed as base64 ciphertext. Invalid
            // ciphertext must not become an application message.
            std::string payload = m_message;
            if (m_action == "broadcast_encrypted")
            {
                const auto key = crypto::deriveMessageKey(api.getClientPassword(), m_room_id);
                const auto sealed = crypto::fromBase64(m_message);
                if (key.empty() || sealed.empty() || !crypto::decrypt(key, sealed, payload))
                {
                    Log::error("broadcast_encrypted: failed to decrypt message");
                    return ReadResult::failure;
                }
            }

            for (auto&& room = rooms.begin(); room != rooms.end(); room++)
            {
                ClientSession* sender = nullptr;
                for (auto& client : room->getClients())
                {
                    if (client.getId() == m_client_id)
                    {
                        sender = &client;
                    }
                }

                if (room->getId() == m_room_id)
                {
                    Room::Communication newMessage(payload, sender, m_replayed);
                    [[maybe_unused]] auto message_id = newMessage.getId();
                    room->addMessage(std::move(newMessage));

                    assert(room->containsCommunication(message_id));
                    return ReadResult::success;
                }
            }
            return ReadResult::failure;
        }
        else if (m_action == "multicast")
        {
            // Multicast implementation would go here
            return ReadResult::not_implemented;
        }
        return ReadResult::failure;
    }

private:
    std::string m_action;
    uint64_t m_room_id;
    uint64_t m_client_id;
    std::string m_message;
    bool m_replayed = false;
};

} // namespace nexilis::client

#endif
