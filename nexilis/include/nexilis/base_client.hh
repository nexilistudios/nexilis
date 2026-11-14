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

#ifndef NEXILIS_BASE_CLIENT_HH
#define NEXILIS_BASE_CLIENT_HH

#include <atomic>
#include <cassert>
#include <cstdint>
#include <mutex>
#include <string>

#include <nexilis/object/object_2d.hh>
#include <nexilis/object/object_3d.hh>

namespace nexilis
{

/// BaseClient offers common functionality between server- and clientside client objects.
class BaseClient
{
public:
    /// Constructor.
    explicit BaseClient(uint64_t id);

    /// Move constructor.
    BaseClient(BaseClient&& other) noexcept;

    /// Move assignment operator.
    BaseClient& operator=(BaseClient&& other) noexcept;

    /// Copy constructor.
    BaseClient(const BaseClient& other)
        : m_id(other.m_id.load()),
          m_usernameMutex(),
          m_object2D(other.m_object2D),
          m_object3D(other.m_object3D)
    {
        std::lock_guard lock(other.m_usernameMutex);
        m_username = other.m_username;
    }

    /// Copy assignment operator.
    BaseClient& operator=(const BaseClient& other)
    {
        if (this != &other)
        {
            m_id.store(other.m_id.load());
            {
                std::scoped_lock lock(m_usernameMutex, other.m_usernameMutex);
                m_username = other.m_username;
            }
            m_object2D = other.m_object2D;
            m_object3D = other.m_object3D;
        }
        return *this;
    }

    uint64_t getId() const
    {
        return m_id.load();
    }

    std::string getUsername() const
    {
        std::lock_guard lock(m_usernameMutex);
        return m_username;
    }

    Object2D& getObject2D()
    {
        return m_object2D;
    }

    Object3D& getObject3D()
    {
        return m_object3D;
    }

protected:
    void setId(uint64_t id)
    {
        m_id.store(id);
    }

    void setBaseUsername(const std::string& username)
    {
        std::lock_guard lock(m_usernameMutex);
        m_username = username;
    }

private:
    std::atomic<uint64_t> m_id;
    mutable std::mutex m_usernameMutex;
    std::string m_username;
    Object2D m_object2D;
    Object3D m_object3D;
};

} // namespace nexilis

#endif
