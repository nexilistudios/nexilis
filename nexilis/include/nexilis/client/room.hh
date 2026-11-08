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

#ifndef NEXILIS_CLIENT_ROOM_HH
#define NEXILIS_CLIENT_ROOM_HH

#include <algorithm>

#include <nexilis/base_room.hh>
#include <nexilis/client/client_session.hh>

namespace nexilis::client
{

class Room : public BaseRoom
{
public:
    /// Communication type for communications in a room.
    class Communication
    {
    public:
        /// Constructor.
        /// \param payload The data for the Communication messages.
        Communication(const std::string& payload, ClientSession* sender);

        /// Copy constructor.
        Communication(const Communication& other);

        /// Copy assignment.
        Communication& operator=(const Communication& other);

        /// Move constructor.
        Communication(Communication&& other);

        /// Move assignment operator.
        Communication& operator=(Communication&& other);

        /// Comparison operator overload.
        friend bool operator==(const Communication& lhs, const Communication& rhs);

        /// Non-comparison operator overload.
        friend bool operator!=(const Communication& lhs, const Communication& rhs)
        {
            return !(lhs == rhs);
        }

        /// Get the payload data as a string.
        const std::string& getPayload() const
        {
            return m_payload;
        }

        /// Get client identification.
        const ClientSession* getClient() const
        {
            return m_client.get();
        }

        uint64_t getSenderId() const
        {
            return m_senderId;
        }

        uint64_t getId() const
        {
            return m_id;
        }

    private:
        /// The data of the communication.
        std::string m_payload;

        /// The id of the message.
        std::shared_ptr<const ClientSession> m_client;
        uint64_t m_senderId;

        /// The id of the message.
        uint64_t m_id;
    };

    /// Constructor.
    explicit Room(const RoomData& roomData, std::vector<ClientSession>&& clients);

    /// Copy constructor.
    Room(const Room& other);

    /// Move constructor.
    Room(Room&& other) noexcept;

    /// Copy assignment operator.
    Room& operator=(const Room& other) = delete;

    /// Move assignment operator.
    Room& operator=(Room&& other) noexcept;

    /// Comparison operator overload.
    friend bool operator==(const Room& lhs, const Room& rhs);

    /// Non-comparison operator overload.
    friend bool operator!=(const Room& lhs, const Room& rhs)
    {
        return !(lhs == rhs);
    }

    void addClient(ClientSession&& client)
    {
        const auto& clientId = client.getId();
        const auto duplicate = std::find_if(m_clients.begin(), m_clients.end(),
                                            [&clientId](const ClientSession& session)
                                            { return session.getId() == clientId; });
        if (duplicate != m_clients.end())
            return;

        m_clients.emplace_back(std::move(client));
    }

    void removeClient(uint64_t clientId)
    {
        m_clients.erase(std::remove_if(m_clients.begin(), m_clients.end(),
                                       [&clientId](const ClientSession& session)
                                       { return clientId == session.getId(); }),
                        m_clients.end());
    }

    std::vector<ClientSession>& getClients()
    {
        return m_clients;
    }

    const std::vector<ClientSession>& getClients() const
    {
        return m_clients;
    }

    void addMessage(Room::Communication&& broadcast)
    {
        m_roomMessages.emplace_back(std::move(broadcast));
    }

    const std::vector<Room::Communication>& getMessages() const
    {
        return m_roomMessages;
    }

    bool containsCommunication(const Room::Communication& communication);
    bool containsCommunication(uint64_t communicationId);

private:
    /// All of the clients currently inside this room.
    std::vector<ClientSession> m_clients;

    /// All of the broadcasts that have been sent in this room.
    std::vector<Room::Communication> m_roomMessages;
};

} // namespace nexilis::client

#endif
