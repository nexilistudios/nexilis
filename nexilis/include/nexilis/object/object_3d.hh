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

#ifndef NEXILIS_OBJECT3D_HH
#define NEXILIS_OBJECT3D_HH

#include <nexilis/object/object.hh>
#include <nexilis/types/vector3.hh>

#include <atomic>

namespace nexilis
{

class Object3D : public Object<Vector3f>
{
public:
    /// Constructor.
    explicit Object3D(uint64_t id, const Vector3f& position = Vector3f(), const Vector3f& dimensions = Vector3f(1.f, 1.f, 1.f))
        : Object(id, position, dimensions)
    {
    }

    /// Copy constructor.
    Object3D(const Object3D& other)
        : Object(other), m_yaw(other.getYaw())
    {
    }

    /// Copy assignment operator.
    Object3D& operator=(const Object3D& other)
    {
        if (this != &other)
        {
            Object::operator=(other);
            m_yaw.store(other.getYaw());
        }
        return *this;
    }

    /// Move constructor.
    Object3D(Object3D&& other) noexcept;

    /// Move assignment operator.
    Object3D& operator=(Object3D&& other) noexcept;

    nx_data getData() override
    {
        return baseData();
    }

    void setYaw(float yaw)
    {
        m_yaw.store(yaw);
    }

    float getYaw() const
    {
        return m_yaw.load();
    }

private:
    std::atomic<float> m_yaw{0.0f};
};

} // namespace nexilis

#endif
