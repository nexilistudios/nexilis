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

#include <nexilis/server/message/base_message.hh>

namespace nexilis::server
{

BaseMessage::Data::Data(uint64_t messageId, const std::string& address, std::shared_ptr<User> user)
    : m_messageId(messageId),
      m_address(address),
      m_user(std::move(user))
{
}

BaseMessage::Data::Data(Data&& other) noexcept
    : m_messageId(std::move(other.m_messageId)),
      m_address(std::move(other.m_address)),
      m_user(std::move(other.m_user))
{
}

BaseMessage::Data& BaseMessage::Data::operator=(Data&& other) noexcept
{
    if (this != &other)
    {
        m_messageId = std::move(other.m_messageId);
        m_address = std::move(other.m_address);
        m_user = std::move(other.m_user);
        other.m_user = nullptr;
    }
    return *this;
}

uint64_t BaseMessage::Data::getMessageId() const
{
    return m_messageId;
}

const std::string& BaseMessage::Data::getAddress() const
{
    return m_address;
}

User* BaseMessage::Data::getUser() const
{
    return m_user.get();
}

BaseMessage::BaseMessage(Data&& data)
    : m_data(std::move(data))
{
}

BaseMessage::BaseMessage(BaseMessage&& other) noexcept
    : m_data(std::move(other.m_data))
{
}

BaseMessage& BaseMessage::operator=(BaseMessage&& other) noexcept
{
    if (this != &other)
    {
        m_data = std::move(other.m_data);
    }
    return *this;
}

uint64_t BaseMessage::getMessageId() const
{
    return m_data.getMessageId();
}

const std::string& BaseMessage::getAddress() const
{
    return m_data.getAddress();
}

User* BaseMessage::getUser() const
{
    return m_data.getUser();
}

} // namespace nexilis::server
