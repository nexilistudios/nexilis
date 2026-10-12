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

#ifndef NEXILIS_COMMAND_SPEC_HH
#define NEXILIS_COMMAND_SPEC_HH

#include <nexilis/client/command_parser.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/command/command_result.hh>
#include <nexilis/server/command/commands.hh>

namespace nexilis
{

/// Aggregate that composes all command-related objects from both the client
/// and server sides.
class CommandSpec
{
public:
    /// Minimal concrete NXProtocol for testing.
    class TestProtocol : public NXProtocol
    {
    public:
        void start() override
        {
        }
        void stop() override
        {
        }
        NXProtocol::Type getType() override
        {
            return NXProtocol::Type::UNKNOWN;
        }
    };

    // Server-side state (declaration order = init order)
    server::ServerConfig settings;
    TestProtocol protocol;
    server::User user{1, "127.0.0.1"};
    server::Command command{settings};

    // Client-side state
    client::ClientConfig serverData;
    client::ClientAPI clientApi{serverData};

    /// Pack server-side args from raw command bytes.
    server::DefaultArgs args(const nx_data& data, size_t messageId = 0)
    {
        return server::DefaultArgs(user, protocol, data, messageId);
    }

    /// Shorthand for server-side command dispatch.
    server::CommandResult read(const nx_data& data, size_t messageId = 0)
    {
        return command.read(data, user, protocol, messageId);
    }

    /// Parse a JSON object into a client-side command handler.
    static std::unique_ptr<client::BaseAPICommand> parse(const boost::json::object& json)
    {
        return client::CommandParser::parse(json);
    }
};

} // namespace nexilis

#endif
