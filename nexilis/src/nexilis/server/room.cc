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

#include <nexilis/logger/log.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/room.hh>
#include <nexilis/util.hh>

namespace nexilis::server
{

Room::Room(const RoomData& data)
    : BaseRoom(data)
{
}

Room::Room(Room&& other)
    : BaseRoom(std::move(other)),
      m_clientIds(std::move(other.m_clientIds)),
      m_broadcasts(std::move(other.m_broadcasts)),
      m_playerHealth(std::move(other.m_playerHealth)),
      m_playerKills(std::move(other.m_playerKills)),
      m_playerDeaths(std::move(other.m_playerDeaths)),
      m_playerTeams(std::move(other.m_playerTeams)),
      m_defaultHealth(other.m_defaultHealth),
      m_deathHandler(std::move(other.m_deathHandler)),
      m_teamHandler(std::move(other.m_teamHandler)),
      m_actionHandler(std::move(other.m_actionHandler)),
      m_combatHandler(std::move(other.m_combatHandler)),
      m_teamSelectionHandler(std::move(other.m_teamSelectionHandler)),
      m_overlappingAllowed(other.m_overlappingAllowed.load())
{
}

Room& Room::operator=(Room&& other)
{
    if (this != &other)
    {
        m_clientIds = std::move(other.m_clientIds);
        m_broadcasts = std::move(other.m_broadcasts);
        m_playerHealth = std::move(other.m_playerHealth);
        m_playerKills = std::move(other.m_playerKills);
        m_playerDeaths = std::move(other.m_playerDeaths);
        m_playerTeams = std::move(other.m_playerTeams);
        m_defaultHealth = other.m_defaultHealth;
        m_deathHandler = std::move(other.m_deathHandler);
        m_teamHandler = std::move(other.m_teamHandler);
        m_actionHandler = std::move(other.m_actionHandler);
        m_combatHandler = std::move(other.m_combatHandler);
        m_teamSelectionHandler = std::move(other.m_teamSelectionHandler);
        m_overlappingAllowed.store(other.m_overlappingAllowed.load());
        BaseRoom::operator=(std::move(other));
    }
    return *this;
}

bool Room::contains(uint64_t userId)
{
    // Use std::any_of algorithm to check if the user is in the room.
    return std::any_of(m_clientIds.begin(), m_clientIds.end(),
                       [&userId](uint64_t id)
                       { return userId == id; });
}

void Room::joinRoom(uint64_t userId)
{
    if (contains(userId))
    {
        Log::warning("User already in this room!");
    }
    else
    {
        m_clientIds.emplace_back(userId);
        Log::info("New user in room: ", getId(), " user: ", userId);
    }
}

void Room::leaveRoom(uint64_t userId)
{
    m_clientIds.erase(std::remove_if(m_clientIds.begin(), m_clientIds.end(),
                                     [&userId](uint64_t id)
                                     { return userId == id; }),
                      m_clientIds.end());
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_playerHealth.erase(userId);
    m_playerKills.erase(userId);
    m_playerDeaths.erase(userId);
    m_playerTeams.erase(userId);
}

void Room::addBroadcast(uint64_t clientId, const std::string& message)
{
    m_broadcasts.emplace_back(Broadcast{clientId, message});
}

float Room::getPlayerHealth(uint64_t clientId) const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    auto it = m_playerHealth.find(clientId);
    if (it != m_playerHealth.end())
        return it->second;
    return m_defaultHealth;
}

bool Room::damagePlayer(uint64_t clientId, float damage)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);

    auto it = m_playerHealth.find(clientId);
    float health;
    if (it != m_playerHealth.end())
    {
        // Ignore damage to players who are already dead. This is what prevents
        // a single kill from being recorded twice: near-simultaneous shots from
        // multiple clients all see health <= 0 and only the first one reports
        // a death.
        if (it->second <= 0.0f)
            return false;
        health = std::max(0.0f, it->second - damage);
        it->second = health;
    }
    else
    {
        health = std::max(0.0f, m_defaultHealth - damage);
        m_playerHealth[clientId] = health;
    }

    return health <= 0.0f;
}

void Room::resetPlayerHealth(uint64_t clientId)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_playerHealth[clientId] = m_defaultHealth;
}

void Room::recordKill(uint64_t killerId, uint64_t victimId)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_playerKills[killerId] += 1;
    m_playerDeaths[victimId] += 1;
}

uint64_t Room::getPlayerKills(uint64_t clientId) const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    auto it = m_playerKills.find(clientId);
    if (it != m_playerKills.end())
        return it->second;
    return 0;
}

uint64_t Room::getPlayerDeaths(uint64_t clientId) const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    auto it = m_playerDeaths.find(clientId);
    if (it != m_playerDeaths.end())
        return it->second;
    return 0;
}

void Room::setPlayerTeam(uint64_t clientId, const std::string& team)
{
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_playerTeams[clientId] = team;
    }
    if (m_teamHandler)
        m_teamHandler(*this, clientId);
}

std::string Room::getPlayerTeam(uint64_t clientId) const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    auto it = m_playerTeams.find(clientId);
    if (it != m_playerTeams.end())
        return it->second;
    return "";
}

void Room::setDeathHandler(DeathHandler handler)
{
    m_deathHandler = std::move(handler);
}

void Room::setTeamHandler(TeamHandler handler)
{
    m_teamHandler = std::move(handler);
}
void Room::setActionHandler(ActionHandler handler)
{
    m_actionHandler = std::move(handler);
}
bool Room::onPlayerAction(uint64_t playerId, uint8_t action)
{
    return m_actionHandler && m_actionHandler(*this, playerId, action);
}
void Room::setCombatHandler(CombatHandler handler)
{
    m_combatHandler = std::move(handler);
}
bool Room::canDamage(uint64_t shooterId, uint64_t targetId) const
{
    return !m_combatHandler || m_combatHandler(shooterId, targetId);
}
void Room::setTeamSelectionHandler(TeamSelectionHandler handler)
{
    m_teamSelectionHandler = std::move(handler);
}
bool Room::canSelectTeam(uint64_t playerId) const
{
    return !m_teamSelectionHandler || m_teamSelectionHandler(playerId);
}

void Room::onPlayerDied(uint64_t killerId, uint64_t victimId)
{
    if (m_deathHandler)
    {
        m_deathHandler(*this, killerId, victimId);
    }
}

bool Room::broadcastToAll(const nx_data& data)
{
    bool all_success = true;
    for (auto clientId : m_clientIds)
    {
        auto* client = ClientStorage::getClientById(clientId);
        if (!client)
            continue;

        if (client->isBoostTCPSet())
        {
            if (!client->boostTCPSend(data))
                all_success = false;
        }
        else if (client->isBoostUDPSet())
        {
            if (!client->boostUDPSend(data))
                all_success = false;
        }
        else if (client->isUnixStreamSet())
        {
            if (!client->unixStreamSend(data))
                all_success = false;
        }
        else if (client->isInetTCPSet())
        {
            if (!client->inetTCPSend(data))
                all_success = false;
        }
        else if (client->isInetUDPSet())
        {
            if (!client->inetUDPSend(data))
                all_success = false;
        }
        else
        {
            all_success = false;
        }
    }
    return all_success;
}

} // namespace nexilis::server
