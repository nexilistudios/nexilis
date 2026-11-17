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

#include <nexilis/convert_to_map.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/command/commands.hh>
#include <nexilis/server/server_json.hh>

namespace nexilis::server
{

void merge_boost_json(const boost::json::object& from, boost::json::object& to)
{
    for (const auto& [key, value] : from)
    {
        if (!to.contains(key))
        {
            to[key] = value;
        }
    }
}

CommandResult ServerImpl::get_info_general(const DefaultArgs& args)
{
    Log::debug("get_info_general: ", args);

    auto room_data = ServerJson::getRoomData(&args.getUser());
    auto client_data = ServerJson::getClientData(&args.getUser());
    merge_boost_json(client_data, room_data);
    auto params = convert_to_map(room_data);
    auto data = Command::clientMessageData(CommandType::getting, "info_general", args.getMessageId(), params);

    if (Command::sendMessageToClient(data, args.getUser(), args.getProtocol()))
    {
        return CommandResult::success;
    }
    else
    {
        return CommandResult::error;
    }
}

} // namespace nexilis::server
