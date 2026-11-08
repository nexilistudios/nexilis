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

#ifndef NEXILIS_CLIENT_STORAGE_HH
#define NEXILIS_CLIENT_STORAGE_HH

#include <nexilis/server/user.hh>

#include <memory>
#include <mutex>
#include <vector>

namespace nexilis::server
{

/// Static lifetime for the clients in the server context.
class ClientStorage
{
public:
    /// Publish a user; reject null handles and duplicate IDs.
    /// A unique_ptr may be moved in and converted to shared ownership.
    static bool add(std::shared_ptr<User> client);

    static bool contains(uint64_t id);

    /// Snapshot of owning handles. This does not lock mutable user fields.
    static std::vector<std::shared_ptr<User>> getAllClients();

    /// Retain the returned handle throughout the operation on the user.
    static std::shared_ptr<User> getClientById(uint64_t id);

    /// Diagnostic lookup only. IP addresses are not session identities.
    static std::vector<std::shared_ptr<User>> getClientsByIpAddress(const std::string& ip_address);

    static bool remove(uint64_t id);

    /// Change a registered user's ID under the registry lock; reject collisions.
    static bool updateId(User& user, uint64_t id);

    static void clear();

private:
    static std::vector<std::shared_ptr<User>> m_clients;
    static std::mutex m_mutex;
};

} // namespace nexilis::server

#endif
