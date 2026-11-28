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
#include <nexilis/nexilis_constants.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/room.hh>
#include <nexilis/util.hh>

#include <algorithm>
#include <stdexcept>

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
      m_broadcastHistoryEnabled(other.m_broadcastHistoryEnabled),
      m_playerHealth(std::move(other.m_playerHealth)),
      m_playerKills(std::move(other.m_playerKills)),
      m_playerDeaths(std::move(other.m_playerDeaths)),
      m_playerTeams(std::move(other.m_playerTeams)),
      m_availableTeams(std::move(other.m_availableTeams)),
      m_defaultHealth(other.m_defaultHealth),
      m_deathHandler(std::move(other.m_deathHandler)),
      m_admittedUsers(std::move(other.m_admittedUsers)),
      m_admissionHandler(std::move(other.m_admissionHandler)),
      m_teamHandler(std::move(other.m_teamHandler)),
      m_actionHandler(std::move(other.m_actionHandler)),
      m_combatHandler(std::move(other.m_combatHandler)),
      m_teamSelectionHandler(std::move(other.m_teamSelectionHandler)),
      m_movementInputHandler(std::move(other.m_movementInputHandler)),
      m_movement2DInputHandler(std::move(other.m_movement2DInputHandler)),
      m_position3DHandler(std::move(other.m_position3DHandler)),
      m_position2DHandler(std::move(other.m_position2DHandler)),
      m_dimension3DHandler(std::move(other.m_dimension3DHandler)),
      m_dimension2DHandler(std::move(other.m_dimension2DHandler)),
      m_shotHandler(std::move(other.m_shotHandler)),
      m_shot2DHandler(std::move(other.m_shot2DHandler)),
      m_audioEventHandler(std::move(other.m_audioEventHandler)),
      m_spawnHandler(std::move(other.m_spawnHandler)),
      m_gameItemPickupHandler(std::move(other.m_gameItemPickupHandler)),
      m_object2DCreationHandler(std::move(other.m_object2DCreationHandler)),
      m_object3DCreationHandler(std::move(other.m_object3DCreationHandler)),
      m_movingObject2DCreationHandler(std::move(other.m_movingObject2DCreationHandler)),
      m_movingObject3DCreationHandler(std::move(other.m_movingObject3DCreationHandler)),
      m_gameItemCreationHandler(std::move(other.m_gameItemCreationHandler)),
      m_object2DMutationHandler(std::move(other.m_object2DMutationHandler)),
      m_object3DMutationHandler(std::move(other.m_object3DMutationHandler)),
      m_gameItemMutationHandler(std::move(other.m_gameItemMutationHandler)),
      m_broadcastHandler(std::move(other.m_broadcastHandler)),
      m_clientPositionAllowed(other.m_clientPositionAllowed),
      m_legacyClientMovementAllowed(other.m_legacyClientMovementAllowed.load()),
      m_clientObjectCreationAllowed(other.m_clientObjectCreationAllowed),
      m_clientGameItemCreationAllowed(other.m_clientGameItemCreationAllowed),
      m_clientRoomRemovalAllowed(other.m_clientRoomRemovalAllowed),
      m_overlappingAllowed(other.m_overlappingAllowed.load()),
      m_teamDamageEnabled(other.m_teamDamageEnabled.load())
{
}

Room& Room::operator=(Room&& other)
{
    if (this != &other)
    {
        m_clientIds = std::move(other.m_clientIds);
        m_broadcasts = std::move(other.m_broadcasts);
        m_broadcastHistoryEnabled = other.m_broadcastHistoryEnabled;
        m_playerHealth = std::move(other.m_playerHealth);
        m_playerKills = std::move(other.m_playerKills);
        m_playerDeaths = std::move(other.m_playerDeaths);
        m_playerTeams = std::move(other.m_playerTeams);
        m_availableTeams = std::move(other.m_availableTeams);
        m_defaultHealth = other.m_defaultHealth;
        m_deathHandler = std::move(other.m_deathHandler);
        m_admittedUsers = std::move(other.m_admittedUsers);
        m_admissionHandler = std::move(other.m_admissionHandler);
        m_teamHandler = std::move(other.m_teamHandler);
        m_actionHandler = std::move(other.m_actionHandler);
        m_combatHandler = std::move(other.m_combatHandler);
        m_teamSelectionHandler = std::move(other.m_teamSelectionHandler);
        m_movementInputHandler = std::move(other.m_movementInputHandler);
        m_movement2DInputHandler = std::move(other.m_movement2DInputHandler);
        m_position3DHandler = std::move(other.m_position3DHandler);
        m_position2DHandler = std::move(other.m_position2DHandler);
        m_dimension3DHandler = std::move(other.m_dimension3DHandler);
        m_dimension2DHandler = std::move(other.m_dimension2DHandler);
        m_shotHandler = std::move(other.m_shotHandler);
        m_shot2DHandler = std::move(other.m_shot2DHandler);
        m_audioEventHandler = std::move(other.m_audioEventHandler);
        m_spawnHandler = std::move(other.m_spawnHandler);
        m_gameItemPickupHandler = std::move(other.m_gameItemPickupHandler);
        m_object2DCreationHandler = std::move(other.m_object2DCreationHandler);
        m_object3DCreationHandler = std::move(other.m_object3DCreationHandler);
        m_movingObject2DCreationHandler = std::move(other.m_movingObject2DCreationHandler);
        m_movingObject3DCreationHandler = std::move(other.m_movingObject3DCreationHandler);
        m_gameItemCreationHandler = std::move(other.m_gameItemCreationHandler);
        m_object2DMutationHandler = std::move(other.m_object2DMutationHandler);
        m_object3DMutationHandler = std::move(other.m_object3DMutationHandler);
        m_gameItemMutationHandler = std::move(other.m_gameItemMutationHandler);
        m_broadcastHandler = std::move(other.m_broadcastHandler);
        m_clientPositionAllowed = other.m_clientPositionAllowed;
        m_legacyClientMovementAllowed.store(other.m_legacyClientMovementAllowed.load());
        m_clientObjectCreationAllowed = other.m_clientObjectCreationAllowed;
        m_clientGameItemCreationAllowed = other.m_clientGameItemCreationAllowed;
        m_clientRoomRemovalAllowed = other.m_clientRoomRemovalAllowed;
        m_overlappingAllowed.store(other.m_overlappingAllowed.load());
        m_teamDamageEnabled.store(other.m_teamDamageEnabled.load());
        BaseRoom::operator=(std::move(other));
    }
    return *this;
}

bool Room::contains(uint64_t userId) const
{
    std::lock_guard lock(m_membershipMutex);
    return std::any_of(m_clientIds.begin(), m_clientIds.end(),
                       [&userId](uint64_t id)
                       { return userId == id; });
}

void Room::joinRoom(uint64_t userId)
{
    tryJoinRoom(userId);
}

bool Room::tryJoinRoom(uint64_t userId)
{
    {
        std::lock_guard lock(m_membershipMutex);
        if (std::find(m_clientIds.begin(), m_clientIds.end(), userId) != m_clientIds.end() ||
            m_clientIds.size() >= getMaxSize())
            return false;
        m_clientIds.emplace_back(userId);
    }
    Log::info("New user in room: ", getId(), " user: ", userId);
    return true;
}

void Room::addAdmittedUser(uint64_t userId)
{
    std::lock_guard lock(m_admissionMutex);
    m_admittedUsers.insert(userId);
}

void Room::removeAdmittedUser(uint64_t userId)
{
    std::lock_guard lock(m_admissionMutex);
    m_admittedUsers.erase(userId);
}

bool Room::isAdmittedUser(uint64_t userId) const
{
    std::lock_guard lock(m_admissionMutex);
    return m_admittedUsers.count(userId) != 0;
}

void Room::setAdmissionHandler(AdmissionHandler handler)
{
    std::lock_guard lock(m_admissionMutex);
    m_admissionHandler = std::move(handler);
}

bool Room::allowsUser(const User& user) const
{
    AdmissionHandler handler;
    bool explicitlyAdmitted = false;
    {
        std::lock_guard lock(m_admissionMutex);
        handler = m_admissionHandler;
        explicitlyAdmitted = m_admittedUsers.count(user.getId()) != 0;
    }
    if (!explicitlyAdmitted && user.getId() != getCreatorId() && !contains(user.getId()))
        return false;
    if (!handler)
        return true;
    try
    {
        return handler(user);
    }
    catch (...)
    {
        return false;
    }
}

std::vector<uint64_t> Room::getClients() const
{
    std::lock_guard lock(m_membershipMutex);
    return m_clientIds;
}

void Room::leaveRoom(uint64_t userId)
{
    std::lock_guard selectionLock(m_teamSelectionMutex);
    {
        std::lock_guard lock(m_membershipMutex);
        std::erase(m_clientIds, userId);
    }
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_playerHealth.erase(userId);
    m_playerKills.erase(userId);
    m_playerDeaths.erase(userId);
    m_playerTeams.erase(userId);
}

void Room::setBroadcastHistoryEnabled(bool enabled)
{
    std::lock_guard<std::mutex> lock(m_broadcastMutex);
    m_broadcastHistoryEnabled = enabled;
    if (!enabled)
        m_broadcasts.clear();
}

void Room::addBroadcast(uint64_t clientId, const std::string& message, bool encrypted)
{
    std::lock_guard<std::mutex> lock(m_broadcastMutex);
    if (!m_broadcastHistoryEnabled)
        return;
    if (m_broadcasts.size() >= NEXILIS_MAX_BROADCAST_HISTORY)
        m_broadcasts.pop_front();
    m_broadcasts.emplace_back(Broadcast{clientId, message, encrypted});
}

std::vector<Room::Broadcast> Room::getBroadcasts() const
{
    std::lock_guard<std::mutex> lock(m_broadcastMutex);
    return std::vector<Broadcast>(m_broadcasts.begin(), m_broadcasts.end());
}

void Room::setBroadcastHandler(BroadcastHandler handler)
{
    m_broadcastHandler = std::move(handler);
}

bool Room::canBroadcast(uint64_t playerId, const std::string& message, bool encrypted) const
{
    return m_broadcastHandler && m_broadcastHandler(playerId, message, encrypted);
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

void Room::resetPlayerStats()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_playerKills.clear();
    m_playerDeaths.clear();
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
        std::lock_guard selectionLock(m_teamSelectionMutex);
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
    return m_combatHandler && m_combatHandler(shooterId, targetId);
}
void Room::setTeamSelectionHandler(TeamSelectionHandler handler)
{
    std::lock_guard lock(m_teamSelectionMutex);
    m_teamSelectionHandler = std::move(handler);
}
void Room::setAvailableTeams(std::vector<std::string> teams)
{
    if (std::any_of(teams.begin(), teams.end(), [](const std::string& team)
                    { return team.empty() || team.size() > 64 ||
                             team.find('\0') != std::string::npos; }))
        throw std::invalid_argument("Invalid team name");
    auto sorted = teams;
    std::sort(sorted.begin(), sorted.end());
    if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end())
        throw std::invalid_argument("Duplicate team name");
    std::lock_guard selectionLock(m_teamSelectionMutex);
    std::lock_guard lock(m_stateMutex);
    m_availableTeams = std::move(teams);
}
std::vector<std::string> Room::getAvailableTeams() const
{
    std::lock_guard lock(m_stateMutex);
    return m_availableTeams;
}
bool Room::canSelectTeam(uint64_t playerId, const std::string& team) const
{
    std::lock_guard selectionLock(m_teamSelectionMutex);
    if (!m_teamSelectionHandler)
        return false;
    {
        std::lock_guard lock(m_stateMutex);
        if (std::find(m_availableTeams.begin(), m_availableTeams.end(), team) ==
            m_availableTeams.end())
            return false;
    }
    return m_teamSelectionHandler(playerId, team);
}
bool Room::trySelectTeam(uint64_t playerId, const std::string& team)
{
    {
        std::lock_guard selectionLock(m_teamSelectionMutex);
        if (!contains(playerId) || !m_teamSelectionHandler)
            return false;
        {
            std::lock_guard stateLock(m_stateMutex);
            if (std::find(m_availableTeams.begin(), m_availableTeams.end(), team) ==
                m_availableTeams.end())
                return false;
        }
        if (!m_teamSelectionHandler(playerId, team))
            return false;
        {
            std::lock_guard stateLock(m_stateMutex);
            m_playerTeams[playerId] = team;
        }
    }
    if (m_teamHandler)
        m_teamHandler(*this, playerId);
    return true;
}
void Room::setClientPositionAllowed(bool allowed)
{
    m_clientPositionAllowed = allowed;
}
bool Room::isClientPositionAllowed() const
{
    return m_clientPositionAllowed;
}
void Room::setPosition3DHandler(Position3DHandler handler)
{
    m_position3DHandler = std::move(handler);
}
bool Room::canSetPosition3D(uint64_t playerId, const Vector3f& position, float yaw) const
{
    return m_position3DHandler && m_position3DHandler(playerId, position, yaw);
}
void Room::setPosition2DHandler(Position2DHandler handler)
{
    m_position2DHandler = std::move(handler);
}
bool Room::canSetPosition2D(uint64_t playerId, const Vector2f& position) const
{
    return m_position2DHandler && m_position2DHandler(playerId, position);
}
void Room::setDimension3DHandler(Dimension3DHandler handler)
{
    m_dimension3DHandler = std::move(handler);
}
bool Room::canSetDimensions3D(uint64_t playerId, const Vector3f& dimensions) const
{
    return m_dimension3DHandler && m_dimension3DHandler(playerId, dimensions);
}
void Room::setDimension2DHandler(Dimension2DHandler handler)
{
    m_dimension2DHandler = std::move(handler);
}
bool Room::canSetDimensions2D(uint64_t playerId, const Vector2f& dimensions) const
{
    return m_dimension2DHandler && m_dimension2DHandler(playerId, dimensions);
}
void Room::setMovementInputHandler(MovementInputHandler handler)
{
    m_movementInputHandler = std::move(handler);
}
bool Room::onMovementInput(uint64_t playerId, const Vector3f& input, float yaw)
{
    return m_movementInputHandler && m_movementInputHandler(playerId, input, yaw);
}
bool Room::hasMovementInputHandler() const
{
    return static_cast<bool>(m_movementInputHandler);
}
void Room::setMovement2DInputHandler(Movement2DInputHandler handler)
{
    m_movement2DInputHandler = std::move(handler);
}
bool Room::onMovement2DInput(uint64_t playerId, const Vector2f& input)
{
    return m_movement2DInputHandler && m_movement2DInputHandler(playerId, input);
}
bool Room::hasMovement2DInputHandler() const
{
    return static_cast<bool>(m_movement2DInputHandler);
}
void Room::setLegacyClientMovementAllowed(bool allowed)
{
    m_legacyClientMovementAllowed.store(allowed);
}
bool Room::isLegacyClientMovementAllowed() const
{
    return m_legacyClientMovementAllowed.load();
}
void Room::setShotHandler(ShotHandler handler)
{
    m_shotHandler = std::move(handler);
}
bool Room::hasShotHandler() const
{
    return static_cast<bool>(m_shotHandler);
}
std::optional<Room::ShotHit> Room::resolveShot(uint64_t shooterId,
                                               const Vector3f& direction,
                                               uint8_t slot, uint8_t action)
{
    return m_shotHandler ? m_shotHandler(shooterId, direction, slot, action) : std::nullopt;
}
void Room::setShot2DHandler(Shot2DHandler handler)
{
    m_shot2DHandler = std::move(handler);
}
bool Room::canShoot2D(uint64_t playerId, const Vector2f& direction) const
{
    return m_shot2DHandler && m_shot2DHandler(playerId, direction);
}
void Room::setAudioEventHandler(AudioEventHandler handler)
{
    m_audioEventHandler = std::move(handler);
}
bool Room::canEmitAudioEvent(uint64_t playerId, uint8_t sound, const Vector3f& position) const
{
    return m_audioEventHandler && m_audioEventHandler(playerId, sound, position);
}
void Room::setSpawnHandler(SpawnHandler handler)
{
    m_spawnHandler = std::move(handler);
}
void Room::spawnPlayer(uint64_t playerId)
{
    if (m_spawnHandler)
        m_spawnHandler(*this, playerId);
}
void Room::setClientObjectCreationAllowed(bool allowed)
{
    m_clientObjectCreationAllowed = allowed;
}
bool Room::isClientObjectCreationAllowed() const
{
    return m_clientObjectCreationAllowed;
}
void Room::setObject2DCreationHandler(Object2DCreationHandler handler)
{
    m_object2DCreationHandler = std::move(handler);
}
bool Room::canCreateObject2D(uint64_t playerId, const Vector2f& position,
                             const Vector2f& dimensions, const std::string& filepath) const
{
    return m_object2DCreationHandler &&
           m_object2DCreationHandler(playerId, position, dimensions, filepath);
}
void Room::setObject3DCreationHandler(Object3DCreationHandler handler)
{
    m_object3DCreationHandler = std::move(handler);
}
bool Room::canCreateObject3D(uint64_t playerId, const Vector3f& position,
                             const Vector3f& dimensions, const std::string& filepath) const
{
    return m_object3DCreationHandler &&
           m_object3DCreationHandler(playerId, position, dimensions, filepath);
}
void Room::setMovingObject2DCreationHandler(MovingObject2DCreationHandler handler)
{
    m_movingObject2DCreationHandler = std::move(handler);
}
bool Room::canCreateMovingObject2D(uint64_t playerId, const Vector2f& start,
                                   const Vector2f& dimensions, const std::string& filepath,
                                   const Vector2f& displacement, float duration,
                                   MovementType type, const Vector2f& destination) const
{
    return m_movingObject2DCreationHandler &&
           m_movingObject2DCreationHandler(playerId, start, dimensions, filepath,
                                           displacement, duration, type, destination);
}
void Room::setMovingObject3DCreationHandler(MovingObject3DCreationHandler handler)
{
    m_movingObject3DCreationHandler = std::move(handler);
}
bool Room::canCreateMovingObject3D(uint64_t playerId, const Vector3f& start,
                                   const Vector3f& dimensions, const std::string& filepath,
                                   const Vector3f& displacement, float duration,
                                   MovementType type, const Vector3f& destination) const
{
    return m_movingObject3DCreationHandler &&
           m_movingObject3DCreationHandler(playerId, start, dimensions, filepath,
                                           displacement, duration, type, destination);
}
void Room::setObject2DMutationHandler(Object2DMutationHandler handler)
{
    m_object2DMutationHandler = std::move(handler);
}
bool Room::canMutateObject2D(uint64_t playerId, uint64_t objectId, ObjectMutation action,
                             const std::optional<Vector2f>& position) const
{
    return m_object2DMutationHandler &&
           m_object2DMutationHandler(playerId, objectId, action, position);
}
void Room::setObject3DMutationHandler(Object3DMutationHandler handler)
{
    m_object3DMutationHandler = std::move(handler);
}
bool Room::canMutateObject3D(uint64_t playerId, uint64_t objectId, ObjectMutation action,
                             const std::optional<Vector3f>& position) const
{
    return m_object3DMutationHandler &&
           m_object3DMutationHandler(playerId, objectId, action, position);
}
void Room::setClientGameItemCreationAllowed(bool allowed)
{
    m_clientGameItemCreationAllowed = allowed;
}
bool Room::isClientGameItemCreationAllowed() const
{
    return m_clientGameItemCreationAllowed;
}
void Room::setGameItemCreationHandler(GameItemCreationHandler handler)
{
    m_gameItemCreationHandler = std::move(handler);
}
bool Room::canCreateGameItem(uint64_t playerId, const Vector3f& position,
                             const Vector3f& dimensions, const std::string& type,
                             const std::string& status, const std::string& filepath) const
{
    return m_gameItemCreationHandler &&
           m_gameItemCreationHandler(playerId, position, dimensions, type, status, filepath);
}
void Room::setGameItemMutationHandler(GameItemMutationHandler handler)
{
    m_gameItemMutationHandler = std::move(handler);
}
bool Room::canMutateGameItem(uint64_t playerId, uint64_t itemId, GameItemMutation action,
                             const std::string& status) const
{
    return m_gameItemMutationHandler &&
           m_gameItemMutationHandler(playerId, itemId, action, status);
}
void Room::setClientRoomRemovalAllowed(bool allowed)
{
    m_clientRoomRemovalAllowed = allowed;
}
bool Room::isClientRoomRemovalAllowed() const
{
    return m_clientRoomRemovalAllowed;
}
void Room::setGameItemPickupHandler(GameItemPickupHandler handler)
{
    m_gameItemPickupHandler = std::move(handler);
}
bool Room::onGameItemPickup(uint64_t playerId, uint64_t itemId)
{
    return m_gameItemPickupHandler && m_gameItemPickupHandler(*this, playerId, itemId);
}
bool Room::hasGameItemPickupHandler() const
{
    return static_cast<bool>(m_gameItemPickupHandler);
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
    for (auto clientId : getClients())
    {
        auto client = ClientStorage::getClientById(clientId);
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

bool Room::tryAddObject(Object2D&& object, std::size_t maximum)
{
    std::lock_guard lock(m_resourceMutex);
    if (getObjects2D().size() + getObjects3D().size() >= maximum)
        return false;
    BaseRoom::addObject(std::move(object));
    return true;
}

void Room::addObject(Object2D&& object)
{
    std::lock_guard lock(m_resourceMutex);
    BaseRoom::addObject(std::move(object));
}

bool Room::tryAddObject(Object3D&& object, std::size_t maximum)
{
    std::lock_guard lock(m_resourceMutex);
    if (getObjects2D().size() + getObjects3D().size() >= maximum)
        return false;
    BaseRoom::addObject(std::move(object));
    return true;
}

void Room::addObject(Object3D&& object)
{
    std::lock_guard lock(m_resourceMutex);
    BaseRoom::addObject(std::move(object));
}

bool Room::tryAddGameItem(GameItem&& object, std::size_t maximum)
{
    std::lock_guard lock(m_resourceMutex);
    if (getGameItems().size() >= maximum)
        return false;
    BaseRoom::addGameItem(std::move(object));
    return true;
}

void Room::addGameItem(GameItem&& object)
{
    std::lock_guard lock(m_resourceMutex);
    BaseRoom::addGameItem(std::move(object));
}

void Room::deleteObject2D(uint64_t id)
{
    std::lock_guard lock(m_resourceMutex);
    BaseRoom::deleteObject2D(id);
}

void Room::deleteObject3D(uint64_t id)
{
    std::lock_guard lock(m_resourceMutex);
    BaseRoom::deleteObject3D(id);
}

void Room::deleteGameItem(uint64_t id)
{
    std::lock_guard lock(m_resourceMutex);
    BaseRoom::deleteGameItem(id);
}

std::vector<Object2D> Room::getObjects2DSnapshot() const
{
    std::lock_guard lock(m_resourceMutex);
    return BaseRoom::getObjects2D();
}

std::vector<Object3D> Room::getObjects3DSnapshot() const
{
    std::lock_guard lock(m_resourceMutex);
    return BaseRoom::getObjects3D();
}

std::vector<GameItem> Room::getGameItemsSnapshot() const
{
    std::lock_guard lock(m_resourceMutex);
    return BaseRoom::getGameItems();
}

bool Room::withObject2D(uint64_t id, const std::function<void(Object2D&)>& action)
{
    std::lock_guard lock(m_resourceMutex);
    auto* object = BaseRoom::getObject2DById(id);
    if (!object)
        return false;
    action(*object);
    return true;
}

bool Room::withObject3D(uint64_t id, const std::function<void(Object3D&)>& action)
{
    std::lock_guard lock(m_resourceMutex);
    auto* object = BaseRoom::getObject3DById(id);
    if (!object)
        return false;
    action(*object);
    return true;
}

bool Room::hasObject2D(uint64_t id)
{
    std::lock_guard lock(m_resourceMutex);
    return BaseRoom::getObject2DById(id) != nullptr;
}

bool Room::hasObject3D(uint64_t id)
{
    std::lock_guard lock(m_resourceMutex);
    return BaseRoom::getObject3DById(id) != nullptr;
}

bool Room::hasGameItem(uint64_t id)
{
    std::lock_guard lock(m_resourceMutex);
    return BaseRoom::getGameItemById(id) != nullptr;
}

void Room::updateGameItemStatus(uint64_t id, const std::string& status)
{
    std::lock_guard lock(m_resourceMutex);
    BaseRoom::updateGameItemStatus(id, status);
}

} // namespace nexilis::server
