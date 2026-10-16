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

#include <nexilis/client/client_api.hh>
#include <nexilis/client/client_session.hh>
#include <nexilis/client/command_parser.hh>
#include <nexilis/client/packet.hh>
#include <nexilis/command_type.hh>
#include <nexilis/json.hh>
#include <nexilis/logger/log.hh>
#include <nexilis/util.hh>

#include <string>
#include <thread>

namespace nexilis::client
{

ClientAPI::ClientAPI(ClientConfig data)
    : NxClass("ClientAPI"),
      m_serverData(data)
{
}

ClientAPI::ClientAPI(ClientAPI&& other)
    : NxClass(std::move(other)),
      m_serverData(std::move(other.m_serverData)),
      m_clientData(std::move(other.m_clientData)),
      m_clientAPIData(std::move(other.m_clientAPIData))
{
}

ClientAPI& ClientAPI::operator=(ClientAPI&& other)
{
    if (this != &other)
    {
        m_serverData = std::move(other.m_serverData);
        m_clientData = std::move(other.m_clientData);
        m_clientAPIData = std::move(other.m_clientAPIData);

        NxClass::operator=(std::move(other));
    }
    return *this;
}

bool ClientAPI::clientInRoom()
{
    auto& mtx = m_clientAPIData.getRoomsMutex();
    std::lock_guard<std::mutex> lock(*mtx);

    auto& rooms = m_clientAPIData.getCurrentlyActiveRooms();
    for (auto r = rooms.begin(); r != rooms.end(); r++)
    {
        auto& clients = r->getClients();
        for (auto c = clients.begin(); c != clients.end(); c++)
        {
            if (c->getId() == getClientData().getClientId())
            {
                return true;
            }
        }
    }
    return false;
}

uint64_t ClientAPI::clientRoomId()
{
    auto& mtx = m_clientAPIData.getRoomsMutex();
    std::lock_guard<std::mutex> lock(*mtx);

    auto& rooms = m_clientAPIData.getCurrentlyActiveRooms();
    for (auto r = rooms.begin(); r != rooms.end(); r++)
    {
        auto& clients = r->getClients();
        for (auto c = clients.begin(); c != clients.end(); c++)
        {
            if (c->getId() == getClientData().getClientId())
            {
                return r->getId();
            }
        }
    }
    return 0;
}

uint64_t ClientAPI::getNewMessageId()
{
    uint64_t newId = Util::getRandomUint64();
    auto& messageIds = m_clientAPIData.getMessageIds();

    if (std::find(messageIds.begin(), messageIds.end(), newId) != messageIds.end())
    {
        return getNewMessageId();
    }
    else
    {
        messageIds.emplace_back(newId);
        return newId;
    }
}

void ClientAPI::addCallback(const std::pair<uint64_t, const std::function<void()>>& callback)
{
    m_clientAPIData.getCallbacks().emplace_back(callback);
}

ReadResult ClientAPI::readCommand(boost::json::object json)
{
    Log::debug(header(), "Received type: ", json["type"], " message");

    auto cmd = CommandParser::parse(json);
    return cmd->execute(*this, m_clientAPIData);
}

std::string ClientAPI::readResultStr(ReadResult res)
{
    switch (res)
    {
        case ReadResult::parsing_failed:
            return "parsing_failed";
        case ReadResult::command_execution:
            return "command_execution";
        case ReadResult::error_in_json_conversion:
            return "error_in_json_conversion";
        case ReadResult::not_found:
            return "not_found";
        case ReadResult::success:
            return "success";
        case ReadResult::client_missing_room:
            return "client_missing_room";
        case ReadResult::failure:
            return "failure";
        case ReadResult::not_implemented:
            return "not_implemented";
        case ReadResult::unauthorized:
            return "unauthorized";
        case ReadResult::invalid_input:
            return "invalid_input";
    }
    return std::string();
}

ReadResult ClientAPI::readMessage(const nx_data& message)
{
    boost::json::object json;
    try
    {
        nx_data messageWithoutLastByte(message.begin(), message.end() - (message.empty() ? 0 : 1));
        json = Json::convertToJSON(messageWithoutLastByte);
    }
    catch (...)
    {
        Util::debugUint8Vector(message);
        return ReadResult::error_in_json_conversion;
    }

    auto result = readCommand(json);

    if (json.find("callback") != json.end() && json["callback"] != 0)
    {
        readCallback(json["callback"]);
    }
    if (result == ReadResult::success)
    {
        return result;
    }
    else
    {
        Log::error("Server returned other than \"success\"");
        Log::error("ReadResult value: ", readResultStr(result));
        Log::error("Received message: ", Util::convertToString(message));
    }
    return result;
}

void ClientAPI::readCallback(boost::json::value callback)
{
    uint64_t cb;
    if (callback.if_uint64())
    {
        cb = static_cast<uint64_t>(callback.as_uint64());
    }
    else if (callback.if_int64())
    {
        cb = static_cast<uint64_t>(callback.as_int64());
    }
    else
    {
        cb = 0;
    }

    // Calling a callback.
    auto& callbacks = m_clientAPIData.getCallbacks();
    for (auto it = callbacks.begin(); it != callbacks.end(); ++it)
    {
        if (it->first == cb)
        {
            it->second();
            it = callbacks.erase(it);
            break;
        }
    }
}

std::function<void()> ClientAPI::waitUntilRoomsCreated(std::promise<void>& promise, const uint16_t max_attempts, const uint16_t timeout)
{
    return [promise_ptr = std::shared_ptr<std::promise<void>>(&promise, [](auto*) {}), max_attempts, timeout, this]()
    {
        try
        {
            int attempts = 0;
            while (attempts++ < max_attempts)
            {
                {
                    if (!m_clientAPIData.getRoomsMutex())
                    {
                        Log::error(header(), "mtx is null in waitUntilRoomsCreated");
                        break;
                    }

                    std::lock_guard<std::mutex> lock(*m_clientAPIData.getRoomsMutex());
                    if (!m_clientAPIData.getCurrentlyActiveRooms().empty())
                    {
                        promise_ptr->set_value();
                        return;
                    }
                }
                Log::debug(header(), "Tried to getActiveRooms data!");
                std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
            }

            // Timeout reached (100 * max_attempts) ms.
            promise_ptr->set_exception(std::make_exception_ptr(
                    std::runtime_error("Timeout waiting for rooms creation")));
        }
        catch (...)
        {
            promise_ptr->set_exception(std::current_exception());
        }
    };
}

Room* ClientAPI::getRoom(uint64_t room_id)
{
    std::lock_guard<std::mutex> lock(*m_clientAPIData.getRoomsMutex());
    for (auto& room : m_clientAPIData.getCurrentlyActiveRooms())
    {
        if (room.getId() == room_id)
        {
            return &room;
        }
    }
    return nullptr;
}

ClientSession* ClientAPI::getClientFromRoom(uint64_t client_id)
{
    auto& mtx = m_clientAPIData.getRoomsMutex();
    std::lock_guard<std::mutex> lock(*mtx);

    ClientSession* returned_client = nullptr;
    auto& rooms = m_clientAPIData.getCurrentlyActiveRooms();
    for (auto&& room = rooms.begin(); room != rooms.end(); room++)
    {
        for (auto& client : room->getClients())
        {
            if (client.getId() == client_id)
            {
                returned_client = &client;
            }
        }
    }
    return returned_client;
}

Vector3f ClientAPI::getClientPosition3D(uint64_t client_id)
{
    auto& mtx = m_clientAPIData.getRoomsMutex();
    std::lock_guard<std::mutex> lock(*mtx);

    auto& rooms = m_clientAPIData.getCurrentlyActiveRooms();
    for (auto&& room = rooms.begin(); room != rooms.end(); room++)
    {
        for (auto& client : room->getClients())
        {
            if (client.getId() == client_id)
            {
                return client.getObject3D().getPosition();
            }
        }
    }
    return Vector3f();
}

std::vector<ClientAPI::RemoteObject3DSnapshot> ClientAPI::getRemoteObjects3DSnapshot(uint64_t room_id)
{
    auto& mtx = m_clientAPIData.getRoomsMutex();
    std::lock_guard<std::mutex> lock(*mtx);

    std::vector<RemoteObject3DSnapshot> result;
    auto& rooms = m_clientAPIData.getCurrentlyActiveRooms();
    for (auto& room : rooms)
    {
        if (room.getId() == room_id)
        {
            for (auto& obj : room.getObjects3D())
            {
                auto pos = obj.getPosition();
                auto dim = obj.getDimensions();
                result.push_back({obj.getId(), pos.x, pos.y, pos.z, dim.x, dim.y, dim.z});
            }
            break;
        }
    }
    return result;
}

std::vector<ClientAPI::RemoteGameItemSnapshot> ClientAPI::getRemoteGameItemsSnapshot(uint64_t room_id)
{
    auto& mtx = m_clientAPIData.getRoomsMutex();
    std::lock_guard<std::mutex> lock(*mtx);

    std::vector<RemoteGameItemSnapshot> result;
    auto& rooms = m_clientAPIData.getCurrentlyActiveRooms();
    for (auto& room : rooms)
    {
        if (room.getId() == room_id)
        {
            for (auto& item : room.getGameItems())
            {
                auto pos = item.getPosition();
                auto dim = item.getDimensions();
                result.push_back({item.getId(), item.getType(),
                                  pos.x, pos.y, pos.z,
                                  dim.x, dim.y, dim.z,
                                  item.getStatus(), item.getFilepath()});
            }
            break;
        }
    }
    return result;
}

std::vector<ClientAPI::RemotePlayerSnapshot> ClientAPI::getRemotePlayersSnapshot(uint64_t room_id, uint64_t my_id)
{
    auto& mtx = m_clientAPIData.getRoomsMutex();
    std::lock_guard<std::mutex> lock(*mtx);

    std::vector<RemotePlayerSnapshot> result;
    auto& rooms = m_clientAPIData.getCurrentlyActiveRooms();
    for (auto& room : rooms)
    {
        if (room.getId() == room_id)
        {
            for (auto& client : room.getClients())
            {
                if (client.getId() == my_id)
                    continue;
                auto pos = client.getObject3D().getPosition();
                auto dim = client.getObject3D().getDimensions();
                result.push_back({client.getId(), pos.x, pos.y, pos.z, dim.x, dim.y, dim.z,
                                  client.getObject3D().getYaw()});
            }
            break;
        }
    }
    return result;
}

bool ClientAPI::IsInetUDPReady()
{
    return getClientData().getClientId() && !getInetUDPServerAddress().empty() && !getUdpToken().empty();
}

bool ClientAPI::isInetTCPReady()
{
    return getClientData().getClientId() && !getInetTCPServerAddress().empty();
}

bool ClientAPI::isBoostTCPReady()
{
    return getClientData().getClientId() && !getBoostTCPServerAddress().empty();
}

bool ClientAPI::isBoostUDPReady()
{
    return getClientData().getClientId() && !getBoostUDPServerAddress().empty() && !getUdpToken().empty();
}

bool ClientAPI::isUnixDgramReady()
{
    return getClientData().getClientId() && !m_serverData.getUnixDgramServerPath().empty();
}

bool ClientAPI::isUnixStreamReady()
{
    return getClientData().getClientId() != 0 && !getUnixStreamPath().empty();
}

void ClientAPI::waitUntilInetUDPReady()
{
    while (!IsInetUDPReady())
    {
        Log::debug("ClientAPI waiting for inetUDP to be initialized...");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void ClientAPI::waitUntilInetTCPReady()
{
    while (!isInetTCPReady())
    {
        Log::debug("ClientAPI waiting for inetTCP to be initialized...");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void ClientAPI::waitUntilBoostTCPReady()
{
    while (!isBoostTCPReady())
    {
        Log::debug("ClientAPI waiting for BoostTCP to be initialized...");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void ClientAPI::waitUntilBoostUDPReady()
{
    while (!isBoostUDPReady())
    {
        Log::debug("ClientAPI waiting for BoostUDP to be initialized...");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void ClientAPI::waitUntilUnixDgramReady()
{
    while (!isUnixDgramReady())
    {
        Log::debug("ClientAPI waiting for unix dgram to be initialized...");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void ClientAPI::waitUntilUnixStreamReady()
{
    while (!isUnixStreamReady())
    {
        Log::debug("ClientAPI waiting for unix stream to be initialized...");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

} // namespace nexilis::client
