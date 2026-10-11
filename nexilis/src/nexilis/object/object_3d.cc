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

#include <nexilis/object/object_3d.hh>

namespace nexilis
{

Object3D::Object3D(Object3D&& other) noexcept
    : Object(std::move(other)), m_yaw(other.getYaw())
{
}

Object3D& Object3D::operator=(Object3D&& other) noexcept
{
    if (this != &other)
    {
        Object::operator=(std::move(other));
        m_yaw.store(other.getYaw());
    }
    return *this;
}

} // namespace nexilis
