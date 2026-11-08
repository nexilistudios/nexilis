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
#include <nexilis/server/command/command.hh>
#include <nexilis/server/command/command_result.hh>
#include <nexilis/server/command/commands.hh>
#include <nexilis/server/room_limits.hh>

namespace nexilis::server
{

CommandResult ServerImpl::set_general_username(const DefaultArgs& args)
{
    Log::debug("setting::general::username", args);
    const auto maximum = RoomLimits::get().getMaxUsernameLength();
    if (maximum == 0 || args.getData().size() < 3 || args.getData().size() - 3 > maximum)
        return CommandResult::invalid_input;
    auto payload = Util::removeAmountOfBytesFromVector(args.getData(), 3);
    std::string username = Util::convertToString(payload);

    // Setting the username for internal client.
    auto client = ClientStorage::getClientById(args.getUser().getId());
    if (client)
    {
        client->setUsername(username);
    }
    else
    {
        return CommandResult::error;
    }

    auto data = Command::clientMessageData(CommandType::setting, "username", args.getMessageId(),
                                           {{"client_id", boost::json::value(args.getUser().getId())},
                                            {"username", boost::json::value(username)}});

    // Notify the room so peers can display this client's username.
    if (client->getRoomId() != 0)
    {
        Command::sendRoomCommand(data, args.getUser(), args.getProtocol());
    }
    else
    {
        Command::sendMessageToClient(data, args.getUser(), args.getProtocol());
    }

    return CommandResult::success;
}

} // namespace nexilis::server
