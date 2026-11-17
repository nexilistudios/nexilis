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

CommandResult ServerImpl::get_info_rooms(const DefaultArgs& args)
{
    Log::debug("get_info_rooms: ", args);

    auto d = ServerJson::getRoomData(&args.getUser());
    auto params = convert_to_map(d);
    auto data = Command::clientMessageData(CommandType::getting, "info_rooms", args.getMessageId(), params);

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
