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

#ifndef NEXILIS_SERVER_ROOM_HH
#define NEXILIS_SERVER_ROOM_HH

#include <nexilis/base_room.hh>
#include <nexilis/object/object_2d.hh>
#include <nexilis/object/object_3d.hh>
#include <nexilis/room_data.hh>
#include <nexilis/server/user.hh>

#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace nexilis::server
{

/// Room objects to be stored in the RoomStorage.
class Room : public BaseRoom
{
public:
    /// Callback invoked when a player dies. The application decides what happens next
    /// (respawn, spectate, round end, etc.).
    /// \param room The room where the death occurred.
    /// \param killerId The id of the player that caused the death.
    /// \param victimId The id of the player that died.
    using DeathHandler = std::function<void(Room& room, uint64_t killerId, uint64_t victimId)>;
    using TeamHandler = std::function<void(Room& room, uint64_t playerId)>;
    using ActionHandler = std::function<bool(Room& room, uint64_t playerId, uint8_t action)>;
    using CombatHandler = std::function<bool(uint64_t shooterId, uint64_t targetId)>;
    using TeamSelectionHandler = std::function<bool(uint64_t playerId)>;
    using MovementInputHandler = std::function<bool(uint64_t playerId, const Vector3f& input, float yaw)>;
    struct ShotHit
    {
        uint64_t targetId;
        float damage;
    };
    using ShotHandler = std::function<std::optional<ShotHit>(uint64_t shooterId,
                                                             const Vector3f& direction,
                                                             uint8_t slot, uint8_t action)>;
    using SpawnHandler = std::function<void(Room& room, uint64_t playerId)>;
    using GameItemPickupHandler = std::function<bool(Room& room, uint64_t playerId, uint64_t itemId)>;

    /// A broadcast message that has been sent in the room.
    struct Broadcast
    {
        /// The id of the user that sent the message.
        uint64_t clientId = 0;
        /// The payload of the message.
        std::string message = "";
    };

    /// Constructor.
    explicit Room(const RoomData& roomData);

    /// Move constructor.
    Room(Room&& other);

    /// Move assignment operator.
    Room& operator=(Room&& other);

    /// Deleted copy constructor.
    Room(const Room&) = delete;

    /// Deleted copy assignment operator.
    Room& operator=(const Room&) = delete;

    /// User joins the room context.
    /// \param userId The id of the user that joins the room.
    void joinRoom(uint64_t userId);

    /// User leaves the room.
    /// \param userId The id the user that leaves the room.
    void leaveRoom(uint64_t userId);

    /// If the room contains certain user.
    /// \param userId The id of the user we are checking.
    bool contains(uint64_t userId);

    std::vector<uint64_t>& getClients()
    {
        return m_clientIds;
    }

    const std::vector<uint64_t>& getClients() const
    {
        return m_clientIds;
    }

    /// Store a broadcast message in the room.
    /// \param clientId The id of the user that sent the message.
    /// \param message The payload of the message.
    void addBroadcast(uint64_t clientId, const std::string& message);

    /// Get all broadcast messages that have been sent in the room.
    const std::vector<Broadcast>& getBroadcasts() const
    {
        return m_broadcasts;
    }

    /// Get the health of a player. Returns default health if not yet tracked.
    float getPlayerHealth(uint64_t clientId) const;

    /// Apply damage to a player. The player's health is clamped to 0.
    /// \return True if this call brought the player down to 0 health (i.e. the
    /// player was alive before this hit). Damaging an already-dead player is a
    /// no-op that returns false, so a single kill/death is never recorded twice.
    bool damagePlayer(uint64_t clientId, float damage);

    /// Reset a player's health to default.
    void resetPlayerHealth(uint64_t clientId);

    /// Record a kill for the killer and a death for the victim.
    /// \param killerId The id of the player that got the kill.
    /// \param victimId The id of the player that died.
    void recordKill(uint64_t killerId, uint64_t victimId);

    /// Get the number of kills a player has made.
    uint64_t getPlayerKills(uint64_t clientId) const;

    /// Get the number of times a player has died.
    uint64_t getPlayerDeaths(uint64_t clientId) const;

    /// Set the team a player is playing on (e.g. "Terrorist").
    void setPlayerTeam(uint64_t clientId, const std::string& team);

    /// Get the team a player is playing on. Empty string if not set.
    std::string getPlayerTeam(uint64_t clientId) const;

    /// Set the callback invoked when a player's health reaches zero.
    /// If no handler is set, nothing happens on death.
    void setDeathHandler(DeathHandler handler);
    void setTeamHandler(TeamHandler handler);
    void setActionHandler(ActionHandler handler);
    bool onPlayerAction(uint64_t playerId, uint8_t action);
    void setCombatHandler(CombatHandler handler);
    bool canDamage(uint64_t shooterId, uint64_t targetId) const;
    void setTeamSelectionHandler(TeamSelectionHandler handler);
    bool canSelectTeam(uint64_t playerId) const;
    /// Allow or disallow direct client position requests
    /// (`room/player_3D/position`). Disallowed by default: a game must
    /// explicitly opt in to client-authoritative position, or install a
    /// movement input handler for server-authoritative movement.
    void setClientPositionAllowed(bool allowed);
    bool isClientPositionAllowed() const;
    void setMovementInputHandler(MovementInputHandler handler);
    bool onMovementInput(uint64_t playerId, const Vector3f& input, float yaw);
    bool hasMovementInputHandler() const;
    /// Required to accept 3D shots; resolves aim requests from server state.
    void setShotHandler(ShotHandler handler);
    bool hasShotHandler() const;
    std::optional<ShotHit> resolveShot(uint64_t shooterId, const Vector3f& direction,
                                       uint8_t slot, uint8_t action);
    void setSpawnHandler(SpawnHandler handler);
    void spawnPlayer(uint64_t playerId);
    void setClientGameItemCreationAllowed(bool allowed);
    bool isClientGameItemCreationAllowed() const;
    void setGameItemPickupHandler(GameItemPickupHandler handler);
    bool onGameItemPickup(uint64_t playerId, uint64_t itemId);
    bool hasGameItemPickupHandler() const;

    /// Notify the room that a player died. Invokes the registered DeathHandler if set.
    /// \param killerId The id of the player that caused the death.
    /// \param victimId The id of the player that died.
    void onPlayerDied(uint64_t killerId, uint64_t victimId);

    /// Send raw data to all clients in this room.
    /// \param data The bytes to send.
    /// \return True if all clients received the data.
    bool broadcastToAll(const nx_data& data);

    /// Can clients in this room share the same position (overlap each other).
    bool isOverlappingAllowed() const
    {
        return m_overlappingAllowed.load();
    }

    /// Set whether clients in this room may share the same position.
    void setOverlappingAllowed(bool allowed)
    {
        m_overlappingAllowed.store(allowed);
    }

private:
    std::vector<uint64_t> m_clientIds;
    std::vector<Broadcast> m_broadcasts;
    std::unordered_map<uint64_t, float> m_playerHealth;
    std::unordered_map<uint64_t, uint64_t> m_playerKills;
    std::unordered_map<uint64_t, uint64_t> m_playerDeaths;
    std::unordered_map<uint64_t, std::string> m_playerTeams;
    float m_defaultHealth = 100.0f;
    DeathHandler m_deathHandler;
    TeamHandler m_teamHandler;
    ActionHandler m_actionHandler;
    CombatHandler m_combatHandler;
    TeamSelectionHandler m_teamSelectionHandler;
    MovementInputHandler m_movementInputHandler;
    ShotHandler m_shotHandler;
    SpawnHandler m_spawnHandler;
    GameItemPickupHandler m_gameItemPickupHandler;
    bool m_clientPositionAllowed = false;
    bool m_clientGameItemCreationAllowed = true;

    /// Guards per-player game state (health, kills, deaths, teams). The server
    /// runs one command thread per client, so these shared maps must be
    /// synchronized to avoid races and double-counted kills.
    mutable std::mutex m_stateMutex;

    /// Atomic because movement threads read this while command threads write it.
    std::atomic<bool> m_overlappingAllowed{true};
};

} // namespace nexilis::server

#endif
