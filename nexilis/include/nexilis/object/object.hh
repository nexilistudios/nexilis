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

#ifndef NEXILIS_OBJECT_HH
#define NEXILIS_OBJECT_HH

#include <memory>
#include <mutex>
#include <nexilis/logger/log.hh>
#include <nexilis/nx_class.hh>
#include <nexilis/nx_util.hh>

namespace nexilis
{

template <typename VectorType>
class Object
{
public:
    /// Constructor.
    Object(uint64_t id, const VectorType& pos, const VectorType& dim)
        : m_id(id),
          m_position(pos),
          m_dimensions(dim),
          m_mutex(std::make_unique<std::mutex>())
    {
    }

    /// Copy constructor.
    Object(const Object& other)
        : m_mutex(std::make_unique<std::mutex>())
    {
        std::lock_guard lock(*other.m_mutex);
        m_id = other.m_id;
        m_position = other.m_position;
        m_dimensions = other.m_dimensions;
        m_filepath = other.m_filepath;
    }

    /// Copy assignment operator.
    Object& operator=(const Object& other)
    {
        if (this != &other)
        {
            if (!m_mutex)
                m_mutex = std::make_unique<std::mutex>();
            std::scoped_lock lock(*m_mutex, *other.m_mutex);
            m_id = other.m_id;
            m_position = other.m_position;
            m_dimensions = other.m_dimensions;
            m_filepath = other.m_filepath;
        }
        return *this;
    }

    /// Move constructor.
    Object(Object&& other) noexcept
        : m_id(std::move(other.m_id)),
          m_position(std::move(other.m_position)),
          m_dimensions(std::move(other.m_dimensions)),
          m_filepath(std::move(other.m_filepath)),
          m_mutex(std::move(other.m_mutex))
    {
    }

    /// Move assignment operator.
    Object& operator=(Object&& other) noexcept
    {
        if (this != &other)
        {
            m_id = std::move(other.m_id);
            m_position = std::move(other.m_position);
            m_dimensions = std::move(other.m_dimensions);
            m_filepath = std::move(other.m_filepath);
            m_mutex = std::move(other.m_mutex);
        }
        return *this;
    }

    /// Virtual destructor.
    virtual ~Object() = default;

    virtual nx_data getData() = 0;

    uint64_t getId() const
    {
        if (m_mutex)
        {
            std::lock_guard lock(*m_mutex);
            return m_id;
        }
        return m_id;
    }

    VectorType getPosition() const
    {
        if (m_mutex)
        {
            std::lock_guard lock(*m_mutex);
            return m_position;
        }
        return m_position;
    }

    VectorType getDimensions() const
    {
        if (m_mutex)
        {
            std::lock_guard lock(*m_mutex);
            return m_dimensions;
        }
        return m_dimensions;
    }

    std::string getFilepath() const
    {
        if (m_mutex)
        {
            std::lock_guard lock(*m_mutex);
            return m_filepath;
        }
        return m_filepath;
    }

    void setPosition(const VectorType& pos)
    {
        if (!m_mutex)
        {
            m_mutex = std::make_unique<std::mutex>();
        }
        std::lock_guard lock(*m_mutex);
        m_position = pos;
    }

    void setDimensions(const VectorType& dim)
    {
        if (!m_mutex)
        {
            m_mutex = std::make_unique<std::mutex>();
        }
        std::lock_guard lock(*m_mutex);
        m_dimensions = dim;
    }

    void setFilepath(const std::string& filepath)
    {
        if (!m_mutex)
        {
            m_mutex = std::make_unique<std::mutex>();
        }
        std::lock_guard lock(*m_mutex);
        m_filepath = filepath;
    }

protected:
    nx_data baseData()
    {
        std::lock_guard lock(*m_mutex);
        nx_data startingData;
        nx_emplace(startingData, m_id, m_position, m_dimensions, m_filepath);
        return startingData;
    }

    /// Object data.
    uint64_t m_id;
    VectorType m_position;
    VectorType m_dimensions;
    std::string m_filepath;

    /// Mutex for setting data.
    std::unique_ptr<std::mutex> m_mutex;
};

} // namespace nexilis

#endif
