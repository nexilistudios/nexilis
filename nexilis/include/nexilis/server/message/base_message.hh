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

#ifndef NEXILIS_MESSAGE_BASE_MESSAGE_HH
#define NEXILIS_MESSAGE_BASE_MESSAGE_HH

#include <memory>
#include <nexilis/server/user.hh>

namespace nexilis::server
{

/// Base class for incoming messages from the client.
class BaseMessage
{
public:
    enum Type
    {
        message,
        auth_message,
        error_message
    };

    class Data
    {
    public:
        /// Constructor.
        explicit Data(uint64_t messageId, const std::string& address, std::shared_ptr<User> user);

        /// Deleted copy constructor.
        Data(const Data&) = delete;

        /// Deleted copy assignment operator.
        Data& operator=(const Data&) = delete;

        /// Move constructor.
        Data(Data&& other) noexcept;

        /// Move assignment operator.
        Data& operator=(Data&& other) noexcept;

        uint64_t getMessageId() const;

        const std::string& getAddress() const;

        User* getUser() const;

    private:
        uint64_t m_messageId;
        std::string m_address;
        uint16_t m_port = 0;
        std::shared_ptr<User> m_user;
    };

    /// Constructor.
    explicit BaseMessage(Data&& data);

    /// Deleted copy constructor.
    BaseMessage(const BaseMessage&) = delete;

    /// Deleted copy assignment operator.
    BaseMessage& operator=(const BaseMessage&) = delete;

    /// Move constructor.
    BaseMessage(BaseMessage&& other) noexcept;

    /// Move assignment operator.
    BaseMessage& operator=(BaseMessage&& other) noexcept;

    /// Virtual destructor.
    virtual ~BaseMessage() = default;

    /// Get the type of the class that inherits BaseMessage.
    virtual Type getType() = 0;

    /// Get the unique identifier of the message.
    uint64_t getMessageId() const;

    /// Get the address where the message is assigned.
    const std::string& getAddress() const;

    /// Borrowed pointer, valid while this message retains its user handle.
    User* getUser() const;

private:
    Data m_data;
};

} // namespace nexilis::server

#endif
