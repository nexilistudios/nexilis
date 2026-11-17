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

#ifndef NEXILIS_CLIENT_COMMAND_SET_PORT_HH
#define NEXILIS_CLIENT_COMMAND_SET_PORT_HH

#include <nexilis/client/base_api_command.hh>
#include <nexilis/udp_auth.hh>

#include <array>
#include <cstdint>
#include <string>

namespace nexilis::client
{

class SetPortCommand : public BaseAPICommand
{
public:
    SetPortCommand(std::string protocol, uint16_t port, std::string switchToken = {})
        : m_protocol(std::move(protocol)), m_port(port), m_switchToken(std::move(switchToken))
    {
    }

    ReadResult execute(ClientAPI& api, ClientAPI::ClientAPIData&) override
    {
        if (!m_switchToken.empty())
        {
            std::array<uint8_t, udp_auth::token_size> parsed{};
            if (m_protocol != "boost_tcp" || !udp_auth::fromHex(m_switchToken, parsed))
                return ReadResult::invalid_input;
            api.setBoostTcpSwitchToken(m_switchToken);
        }
        api.setProtocolPort(m_protocol, m_port);
        return ReadResult::success;
    }

private:
    std::string m_protocol;
    uint16_t m_port;
    std::string m_switchToken;
};

} // namespace nexilis::client

#endif
