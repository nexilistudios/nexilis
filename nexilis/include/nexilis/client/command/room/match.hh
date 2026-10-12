#ifndef NEXILIS_CLIENT_COMMAND_ROOM_MATCH_HH
#define NEXILIS_CLIENT_COMMAND_ROOM_MATCH_HH

#include <nexilis/client/base_api_command.hh>
#include <nexilis/client/client_api.hh>

namespace nexilis::client
{
class RoomMatchCommand : public BaseAPICommand
{
public:
    explicit RoomMatchCommand(std::unique_ptr<ClientAPI::MatchEvent> event)
        : m_event(std::move(event))
    {
    }

    ReadResult execute(ClientAPI&, ClientAPI::ClientAPIData& data) override
    {
        data.pushMatchEvent(std::move(m_event));
        return ReadResult::success;
    }

private:
    std::unique_ptr<ClientAPI::MatchEvent> m_event;
};
} // namespace nexilis::client

#endif
