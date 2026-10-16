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

#ifndef NEXILIS_CLIENT_CLIENT_SESSION_HH
#define NEXILIS_CLIENT_CLIENT_SESSION_HH

#include <nexilis/base_client.hh>
#include <nexilis/types/vector2.hh>

namespace nexilis::client
{

class ClientAPI;

class ClientSession : public BaseClient
{
public:
    /// Constuctor.
    /// \param id The server side id of the client.
    /// \param client_api Pointer to the client api.
    ClientSession(uint64_t id, ClientAPI* client_api);

    /// Virtual destructor.
    virtual ~ClientSession() = default;

    /// Copy constructor.
    ClientSession(const ClientSession& other)
        : BaseClient(other),
          m_clientAPI(other.m_clientAPI)
    {
    }

    /// Move constructor.
    ClientSession(ClientSession&& other) noexcept;

    /// Move assignment operator.
    ClientSession& operator=(ClientSession&& other) noexcept;

    /// Comparison operator overload.
    friend bool operator==(const ClientSession& lhs, const ClientSession& rhs);

    /// Non-comparison operator overload.
    friend bool operator!=(const ClientSession& lhs, const ClientSession& rhs)
    {
        return !(lhs == rhs);
    }

    void setUsername(const std::string& newUsername)
    {
        BaseClient::setBaseUsername(newUsername);
    }

    void setPosition3D(float x, float y, float z);
    Vector3<float> getPosition3D();

    void setPosition2D(float x, float y);
    Vector2<float> getPosition2D();

private:
    /// ClientAPI instance what the client is using.
    ClientAPI* const m_clientAPI = nullptr;
};

} // namespace nexilis::client

#endif
