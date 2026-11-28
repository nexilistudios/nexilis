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

#include <algorithm>
#include <cctype>
#include <mutex>
#include <string_view>

namespace nexilis::server
{

namespace
{
bool hasInvalidUsernameBytes(std::string_view username)
{
    if (username.empty())
        return true;
    return std::any_of(username.begin(), username.end(),
                       [](unsigned char byte)
                       { return byte == '\0' || std::iscntrl(byte); });
}

std::string asciiLower(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    for (unsigned char byte : value)
    {
        const auto lower = byte >= 'A' && byte <= 'Z'
                                   ? static_cast<unsigned char>(byte + ('a' - 'A'))
                                   : byte;
        result.push_back(static_cast<char>(lower));
    }
    return result;
}
} // namespace

CommandResult ServerImpl::set_general_username(const DefaultArgs& args)
{
    Log::debug("setting::general::username", args);
    const auto& limits = RoomLimits::get();
    const auto maximum = limits.getMaxUsernameLength();
    if (maximum == 0 || args.getData().size() < 3 || args.getData().size() - 3 > maximum)
        return CommandResult::invalid_input;
    auto payload = Util::removeAmountOfBytesFromVector(args.getData(), 3);
    std::string username = Util::convertToString(payload);
    if (hasInvalidUsernameBytes(username))
        return CommandResult::invalid_input;

    // Serialize validation and mutation so two clients cannot claim the same
    // name concurrently.
    static std::mutex usernameMutex;
    std::lock_guard lock(usernameMutex);

    const auto normalized = asciiLower(username);
    const auto& reserved = limits.getReservedUsernames();
    if (std::any_of(reserved.begin(), reserved.end(),
                    [&](const std::string& name)
                    { return asciiLower(name) == normalized; }))
        return CommandResult::unauthorized;

    if (limits.areUsernamesUnique())
    {
        for (const auto& client : ClientStorage::getAllClients())
        {
            if (client->getId() == args.getUser().getId())
                continue;
            if (asciiLower(client->getUsername()) == normalized)
                return CommandResult::unauthorized;
        }
    }

    if (const auto& validator = limits.getUsernameValidator())
    {
        try
        {
            if (!validator(username, args.getUser().getAccountId()))
                return CommandResult::unauthorized;
        }
        catch (...)
        {
            Log::error("Username validator failed");
            return CommandResult::error;
        }
    }

    // Setting the username for internal client.
    auto client = ClientStorage::getClientById(args.getUser().getId());
    if (!client)
        return CommandResult::error;
    client->setUsername(username);

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
