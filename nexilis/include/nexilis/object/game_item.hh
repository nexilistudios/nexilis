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

#ifndef NEXILIS_GAME_ITEM_HH
#define NEXILIS_GAME_ITEM_HH

#include <nexilis/nx_data.hh>
#include <nexilis/types/vector3.hh>

#include <memory>
#include <mutex>
#include <string>

namespace nexilis
{

class GameItem
{
public:
    GameItem(uint64_t id, const std::string& type, const Vector3f& position,
             const Vector3f& dimensions, const std::string& status,
             const std::string& filepath = "");

    GameItem(const GameItem& other)
        : m_mutex(std::make_unique<std::mutex>())
    {
        std::lock_guard lock(*other.m_mutex);
        m_id = other.m_id;
        m_type = other.m_type;
        m_position = other.m_position;
        m_dimensions = other.m_dimensions;
        m_status = other.m_status;
        m_filepath = other.m_filepath;
    }

    GameItem& operator=(const GameItem& other)
    {
        if (this != &other)
        {
            if (!m_mutex)
                m_mutex = std::make_unique<std::mutex>();
            std::scoped_lock lock(*m_mutex, *other.m_mutex);
            m_id = other.m_id;
            m_type = other.m_type;
            m_position = other.m_position;
            m_dimensions = other.m_dimensions;
            m_status = other.m_status;
            m_filepath = other.m_filepath;
        }
        return *this;
    }

    GameItem(GameItem&& other) noexcept;
    GameItem& operator=(GameItem&& other) noexcept;

    uint64_t getId() const;
    std::string getType() const;
    Vector3f getPosition() const;
    Vector3f getDimensions() const;
    std::string getStatus() const;
    std::string getFilepath() const;

    void setPosition(const Vector3f& pos);
    void setDimensions(const Vector3f& dim);
    void setStatus(const std::string& status);

private:
    uint64_t m_id;
    std::string m_type;
    Vector3f m_position;
    Vector3f m_dimensions;
    std::string m_status;
    std::string m_filepath;
    mutable std::unique_ptr<std::mutex> m_mutex;
};

} // namespace nexilis

#endif
