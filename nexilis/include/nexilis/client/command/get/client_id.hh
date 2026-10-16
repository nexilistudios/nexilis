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

#ifndef NEXILIS_CLIENT_COMMAND_GET_CLIENT_ID_HH
#define NEXILIS_CLIENT_COMMAND_GET_CLIENT_ID_HH

#include <nexilis/client/base_api_command.hh>
#include <nexilis/client/packet.hh>
#include <nexilis/udp_auth.hh>

#include <mutex>

namespace nexilis::client
{

class GetClientIdCommand : public BaseAPICommand
{
public:
    explicit GetClientIdCommand(uint64_t client_id, std::string udp_token = {})
        : m_client_id(client_id),
          m_udp_token(std::move(udp_token))
    {
    }

    ReadResult execute(ClientAPI& api, ClientAPI::ClientAPIData& data) override
    {
        if (!m_udp_token.empty())
        {
            std::array<uint8_t, udp_auth::token_size> parsed{};
            if (!udp_auth::fromHex(m_udp_token, parsed))
                return ReadResult::invalid_input;
        }
        auto& mtx = data.getRoomsMutex();
        if (!mtx)
            return ReadResult::command_execution;
        std::lock_guard<std::mutex> lock(*mtx);

        api.setClientId(m_client_id);
        if (!m_udp_token.empty())
            api.setUdpToken(m_udp_token);
        data.initialize();

        assert(api.getClientId() == m_client_id);
        assert(data.isInitialized());

        auto p_data = _Packet::clientIdentification(api);
        assert(p_data.size() >= 8);
        assert(Util::convertToType<uint64_t>({p_data.begin(), p_data.begin() + 8}) == api.getClientId());

        return ReadResult::success;
    }

private:
    uint64_t m_client_id;
    std::string m_udp_token;
};

} // namespace nexilis::client

#endif
