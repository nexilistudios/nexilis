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

#include <nexilis/server/client_storage.hh>

#include <algorithm>
#include <iterator>

namespace nexilis::server
{

std::vector<std::shared_ptr<User>> ClientStorage::m_clients;
std::mutex ClientStorage::m_mutex;

bool ClientStorage::add(std::shared_ptr<User> client)
{
    if (!client)
        return false;
    std::lock_guard lock(m_mutex);
    if (std::any_of(m_clients.begin(), m_clients.end(), [&](const auto& existing)
                    { return existing->getId() == client->getId(); }))
        return false;
    m_clients.emplace_back(std::move(client));
    return true;
}

bool ClientStorage::contains(uint64_t id)
{
    return getClientById(id) != nullptr;
}

std::vector<std::shared_ptr<User>> ClientStorage::getAllClients()
{
    std::lock_guard lock(m_mutex);
    return m_clients;
}

std::shared_ptr<User> ClientStorage::getClientById(uint64_t id)
{
    std::lock_guard lock(m_mutex);
    auto it = std::find_if(m_clients.begin(), m_clients.end(),
                           [id](const auto& client)
                           { return client->getId() == id; });
    return it == m_clients.end() ? nullptr : *it;
}

std::vector<std::shared_ptr<User>> ClientStorage::getClientsByIpAddress(const std::string& ip_address)
{
    std::lock_guard lock(m_mutex);
    std::vector<std::shared_ptr<User>> result;
    std::copy_if(m_clients.begin(), m_clients.end(), std::back_inserter(result),
                 [&](const auto& client)
                 { return client->getIPAddress() == ip_address; });
    return result;
}

void ClientStorage::clear()
{
    std::vector<std::shared_ptr<User>> removed;
    {
        std::lock_guard lock(m_mutex);
        removed.swap(m_clients);
    }
    // Explicitly release users after unlocking: callback destructors may
    // reenter the registry.
    removed.clear();
}

bool ClientStorage::updateId(User& user, uint64_t id)
{
    if (!user.hasRootAccess())
        return false;
    std::lock_guard lock(m_mutex);
    bool found = false;
    for (const auto& client : m_clients)
    {
        if (client.get() == &user)
            found = true;
        else if (client->getId() == id)
            return false;
    }
    if (!found)
        return false;
    user.setId(id);
    return true;
}

bool ClientStorage::remove(uint64_t id)
{
    std::shared_ptr<User> removed;
    {
        std::lock_guard lock(m_mutex);
        auto it = std::find_if(m_clients.begin(), m_clients.end(),
                               [id](const auto& client)
                               { return client->getId() == id; });
        if (it == m_clients.end())
            return false;
        removed = std::move(*it);
        m_clients.erase(it);
    }
    // User send callbacks may own resources whose destructors reenter storage.
    // Release the last registry reference after unlocking.
    return true;
}

} // namespace nexilis::server
