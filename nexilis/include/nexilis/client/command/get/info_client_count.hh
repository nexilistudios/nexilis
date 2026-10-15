/* Copyright (C) 2026 Valtteri Viirret
   This file is part of the Nexilis Project.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version. */

#ifndef NEXILIS_CLIENT_COMMAND_GET_INFO_CLIENT_COUNT_HH
#define NEXILIS_CLIENT_COMMAND_GET_INFO_CLIENT_COUNT_HH

#include <nexilis/client/base_api_command.hh>
#include <nexilis/client/client_api.hh>

namespace nexilis::client
{
class GetInfoClientCountCommand : public BaseAPICommand
{
public:
    explicit GetInfoClientCountCommand(uint64_t count)
        : m_count(count)
    {
    }

    ReadResult execute(ClientAPI&, ClientAPI::ClientAPIData& data) override
    {
        data.setServerClientCount(m_count);
        return ReadResult::success;
    }

private:
    uint64_t m_count;
};
} // namespace nexilis::client

#endif
