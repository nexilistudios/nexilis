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

#include <nexilis/auth_config.hh>

namespace nexilis
{

AuthConfig::AuthConfig(AuthConfig&& other)
    : m_mode(std::move(other.m_mode)),
      m_modeLocked(other.m_modeLocked),
      m_password(std::move(other.m_password))
{
}

AuthConfig& AuthConfig::operator=(AuthConfig&& other)
{
    if (this != &other)
    {
        m_mode = std::move(other.m_mode);
        m_modeLocked = other.m_modeLocked;
        m_password = std::move(other.m_password);
    }
    return *this;
}

AuthConfig::AuthConfig(const AuthConfig& other)
    : m_mode(other.m_mode),
      m_modeLocked(other.m_modeLocked),
      m_password(other.m_password)
{
}

AuthConfig& AuthConfig::operator=(const AuthConfig& other)
{
    if (this != &other)
    {
        m_mode = other.m_mode;
        m_modeLocked = other.m_modeLocked;
        m_password = other.m_password;
    }
    return *this;
}

} // namespace nexilis
