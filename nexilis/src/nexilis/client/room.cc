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

#include <nexilis/client/room.hh>

namespace nexilis::client
{

Room::Communication::Communication(const std::string& payload, ClientSession* client)
    : m_payload(payload),
      m_client(client ? std::make_shared<ClientSession>(*client) : nullptr),
      m_senderId(client ? client->getId() : 0),
      m_id(Util::getRandomUint64())
{
}

Room::Communication::Communication(const Communication& other)
    : m_payload(other.m_payload),
      m_client(other.m_client),
      m_senderId(other.m_senderId),
      m_id(other.m_id)
{
}

Room::Communication& Room::Communication::operator=(const Communication& other)
{
    if (this != &other)
    {
        m_payload = other.m_payload;
        m_client = other.m_client;
        m_senderId = other.m_senderId;
        m_id = other.m_id;
    }
    return *this;
}

Room::Communication::Communication(Communication&& other)
    : m_payload(std::move(other.m_payload)),
      m_client(std::move(other.m_client)),
      m_senderId(other.m_senderId),
      m_id(std::move(other.m_id))

{
}

Room::Communication& Room::Communication::operator=(Communication&& other)
{
    if (this != &other)
    {
        m_payload = std::move(other.m_payload);
        m_client = std::move(other.m_client);
        m_senderId = other.m_senderId;
        m_id = std::move(other.m_id);
    }
    return *this;
}

bool operator==(const Room::Communication& lhs, const Room::Communication& rhs)
{
    return lhs.getPayload() == rhs.getPayload() &&
           lhs.getClient() == rhs.getClient() &&
           lhs.getId() == rhs.getId();
}

Room::Room(const Room& other)
    : BaseRoom(other),
      m_clients(other.m_clients),
      m_roomMessages(other.m_roomMessages)
{
}

Room::Room(Room&& other) noexcept
    : BaseRoom(std::move(other)),
      m_clients(std::move(other.m_clients)),
      m_roomMessages(std::move(other.m_roomMessages))
{
}

Room& Room::operator=(Room&& other) noexcept
{
    if (this != &other)
    {
        static_cast<BaseRoom&>(*this) = static_cast<BaseRoom&&>(other);
        m_clients = std::move(other.m_clients);
        m_roomMessages = std::move(other.m_roomMessages);
    }
    return *this;
}

Room::Room(const RoomData& roomData, std::vector<ClientSession>&& clients)
    : BaseRoom(roomData),
      m_clients(std::move(clients))
{
}

bool operator==(const Room& lhs, const Room& rhs)
{
    return lhs.getClients() == rhs.getClients() &&
           lhs.getMessages() == rhs.getMessages();
}

bool Room::containsCommunication(const Room::Communication& communication)
{
    return std::find(m_roomMessages.begin(), m_roomMessages.end(), communication) != m_roomMessages.end();
}

bool Room::containsCommunication(uint64_t communicationId)
{
    auto idComparator = [communicationId](const Communication& item)
    {
        return item.getId() == communicationId;
    };
    return std::find_if(m_roomMessages.begin(), m_roomMessages.end(), idComparator) != m_roomMessages.end();
}

} // namespace nexilis::client
