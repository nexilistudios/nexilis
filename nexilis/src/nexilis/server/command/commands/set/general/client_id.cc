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
#include <nexilis/server/command/commands.hh>

namespace nexilis::server
{

CommandResult ServerImpl::set_general_clientId(const DefaultArgs& args)
{
    Log::debug("setting::general::client_id");
    auto& user = args.getUser();
    auto data = args.getData();
    if (!user.hasRootAccess())
    {
        Log::error("Client needs root access for changing id");
        return CommandResult::unauthorized;
    }
    // We are parsing Command, so remove two bytes from this switch statement.
    auto payload = Util::removeAmountOfBytesFromVector(data, 3);
    uint64_t id = Util::convertToType<uint64_t>(payload);

    return ClientStorage::updateId(user, id)
                   ? CommandResult::success
                   : CommandResult::error;
}

} // namespace nexilis::server
