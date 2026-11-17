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

namespace nexilis::server
{

CommandResult ServerImpl::set_protocol_boosttcp_port(const DefaultArgs& args)
{
    Log::info("Commands::Set::Protocol::BoostTCP::port used!");
    auto payload = Util::removeAmountOfBytesFromVector(args.getData(), 4);
    uint16_t port = Util::convertoToUint16(payload);

    std::map<std::string, boost::json::value> params{
            {"protocol", boost::json::value("boost_tcp")},
            {"port", boost::json::value(port)},
            {"boost_tcp_port", boost::json::value(port)}};
    if (args.getProtocol().getType() == NXProtocol::Type::BOOST_TCP_SERVER)
    {
        const auto switchToken = args.getUser().getBoostTcpSwitchToken();
        if (!switchToken.empty())
            params.emplace("switch_token", switchToken);
    }
    auto data = Command::clientMessageData(CommandType::setting, "port", args.getMessageId(), params);
    if (Command::sendMessageToClient(data, args.getUser(), args.getProtocol()))
    {
        return CommandResult::success;
    }
    return CommandResult::failed_room_send;
}

} // namespace nexilis::server
