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
#include <nexilis/movement_type.hh>
#include <nexilis/object/object_2d.hh>
#include <nexilis/object/object_3d.hh>
#include <nexilis/room_data.hh>
#include <nexilis/server/user.hh>

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

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
    using AdmissionHandler = std::function<bool(const User& user)>;
    using ActionHandler = std::function<bool(Room& room, uint64_t playerId, uint8_t action)>;
    using CombatHandler = std::function<bool(uint64_t shooterId, uint64_t targetId)>;
    using TeamSelectionHandler = std::function<bool(uint64_t playerId, const std::string& team)>;
    using MovementInputHandler = std::function<bool(uint64_t playerId, const Vector3f& input, float yaw)>;
    using Movement2DInputHandler = std::function<bool(uint64_t playerId, const Vector2f& input)>;
    using Position3DHandler = std::function<bool(uint64_t playerId, const Vector3f& position, float yaw)>;
    using Position2DHandler = std::function<bool(uint64_t playerId, const Vector2f& position)>;
    using Dimension3DHandler = std::function<bool(uint64_t playerId, const Vector3f& dimensions)>;
    using Dimension2DHandler = std::function<bool(uint64_t playerId, const Vector2f& dimensions)>;
    using Shot2DHandler = std::function<bool(uint64_t playerId, const Vector2f& direction)>;
    using AudioEventHandler = std::function<bool(uint64_t playerId, uint8_t sound,
                                                 const Vector3f& position)>;
    enum class ObjectMutation
    {
        move,
        destroy
    };
    enum class GameItemMutation
    {
        update,
        destroy
    };
    using Object2DMutationHandler = std::function<bool(uint64_t playerId, uint64_t objectId,
                                                       ObjectMutation action,
                                                       const std::optional<Vector2f>& position)>;
    using Object3DMutationHandler = std::function<bool(uint64_t playerId, uint64_t objectId,
                                                       ObjectMutation action,
                                                       const std::optional<Vector3f>& position)>;
    using GameItemMutationHandler = std::function<bool(uint64_t playerId, uint64_t itemId,
                                                       GameItemMutation action,
                                                       const std::string& status)>;
    using Object2DCreationHandler = std::function<bool(uint64_t playerId,
                                                       const Vector2f& position,
                                                       const Vector2f& dimensions,
                                                       const std::string& filepath)>;
    using Object3DCreationHandler = std::function<bool(uint64_t playerId,
                                                       const Vector3f& position,
                                                       const Vector3f& dimensions,
                                                       const std::string& filepath)>;
    using MovingObject2DCreationHandler = std::function<bool(uint64_t playerId,
                                                             const Vector2f& start, const Vector2f& dimensions, const std::string& filepath,
                                                             const Vector2f& displacement, float duration, MovementType type,
                                                             const Vector2f& destination)>;
    using MovingObject3DCreationHandler = std::function<bool(uint64_t playerId,
                                                             const Vector3f& start, const Vector3f& dimensions, const std::string& filepath,
                                                             const Vector3f& displacement, float duration, MovementType type,
                                                             const Vector3f& destination)>;
    using GameItemCreationHandler = std::function<bool(uint64_t playerId,
                                                       const Vector3f& position,
                                                       const Vector3f& dimensions,
                                                       const std::string& type,
                                                       const std::string& status,
                                                       const std::string& filepath)>;
    using BroadcastHandler = std::function<bool(uint64_t playerId,
                                                const std::string& message, bool encrypted)>;
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
        /// Whether the payload is encrypted and must be decrypted by clients.
        bool encrypted = false;
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

    /// Client creation caps check and insert atomically. Zero disables creation.
    bool tryAddObject(Object2D&& object, std::size_t maximum);
    bool tryAddObject(Object3D&& object, std::size_t maximum);
    bool tryAddGameItem(GameItem&& item, std::size_t maximum);
    /// Server-authored additions bypass caps but share the resource lock.
    void addObject(Object2D&& object);
    void addObject(Object3D&& object);
    void addGameItem(GameItem&& item);
    void deleteObject2D(uint64_t id);
    void deleteObject3D(uint64_t id);
    void deleteGameItem(uint64_t id);
    std::vector<Object2D> getObjects2DSnapshot() const;
    std::vector<Object3D> getObjects3DSnapshot() const;
    std::vector<GameItem> getGameItemsSnapshot() const;
    bool withObject2D(uint64_t id, const std::function<void(Object2D&)>& action);
    bool withObject3D(uint64_t id, const std::function<void(Object3D&)>& action);
    bool hasObject2D(uint64_t id);
    bool hasObject3D(uint64_t id);
    bool hasGameItem(uint64_t id);
    void updateGameItemStatus(uint64_t id, const std::string& status);

    /// User joins the room context.
    /// \param userId The id of the user that joins the room.
    void joinRoom(uint64_t userId);
    /// Atomically enforce the room capacity and insert a new member.
    bool tryJoinRoom(uint64_t userId);
    /// A server policy for joining and discovering this room. An unset handler
    /// makes the room private: only its creator and current members may join or
    /// discover it. Install an explicitly permissive handler for a public room,
    /// or a policy handler for invite/permission-based access.
    void setAdmissionHandler(AdmissionHandler handler);
    bool allowsUser(const User& user) const;

    /// User leaves the room.
    /// \param userId The id the user that leaves the room.
    void leaveRoom(uint64_t userId);

    /// If the room contains certain user.
    /// \param userId The id of the user we are checking.
    bool contains(uint64_t userId) const;

    /// Return a stable snapshot of member IDs.
    std::vector<uint64_t> getClients() const;

    /// Enable storage and join replay of player broadcasts. Disabled by
    /// default; disabling it also clears any stored messages.
    void setBroadcastHistoryEnabled(bool enabled);

    /// Store a broadcast message when history is enabled. Only the most
    /// recent NEXILIS_MAX_BROADCAST_HISTORY messages are retained.
    /// \param clientId The id of the user that sent the message.
    /// \param message The payload of the message.
    void addBroadcast(uint64_t clientId, const std::string& message, bool encrypted = false);

    /// Get a snapshot of the broadcast messages retained in the room.
    std::vector<Broadcast> getBroadcasts() const;
    /// Client broadcasts are denied until an application accepts player-authored
    /// content. The callback must not treat its payload as a server command.
    /// Configure before serving room traffic.
    void setBroadcastHandler(BroadcastHandler handler);
    bool canBroadcast(uint64_t playerId, const std::string& message, bool encrypted) const;

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

    /// Clear kill and death counts when a new match starts in this room.
    void resetPlayerStats();

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
    /// Authorize damage from a server-resolved hit. Missing handlers deny hits.
    /// Validate round state and participants against server-owned state.
    void setCombatHandler(CombatHandler handler);
    bool canDamage(uint64_t shooterId, uint64_t targetId) const;
    /// Whether combat may damage another player on the same team.
    /// Self damage is decided by the application combat handler.
    void setTeamDamageEnabled(bool enabled) { m_teamDamageEnabled.store(enabled); }
    bool isTeamDamageEnabled() const { return m_teamDamageEnabled.load(); }
    /// Set the team names a client may request. Empty by default, so selection
    /// is disabled until the server application configures a list.
    /// Names must be unique, nonempty, at most 64 bytes, and contain no NUL.
    void setAvailableTeams(std::vector<std::string> teams);
    std::vector<std::string> getAvailableTeams() const;
    /// Client team requests require both a configured name and this handler.
    /// The handler may veto the proposed team based on match state. It may read
    /// room state, but must not change team assignments or configuration.
    void setTeamSelectionHandler(TeamSelectionHandler handler);
    /// Check a request without changing the team. Use trySelectTeam for client
    /// requests so approval and assignment cannot race with another request.
    bool canSelectTeam(uint64_t playerId, const std::string& team) const;
    /// Approve and assign a client-requested team as one operation. The policy
    /// callback may inspect room state, including current team assignments.
    bool trySelectTeam(uint64_t playerId, const std::string& team);
    /// Allow or disallow direct client position requests
    /// (`room/player_2D/position` and `room/player_3D/position`). Disallowed by default: a game must
    /// explicitly opt in and install a position handler to validate each
    /// request against server-owned game state.
    void setClientPositionAllowed(bool allowed);
    bool isClientPositionAllowed() const;
    /// Called with the requested in-bounds position before it is stored or broadcast.
    /// Missing handlers reject requests even when client positions are enabled.
    /// Configure handlers before serving room traffic.
    void setPosition3DHandler(Position3DHandler handler);
    bool canSetPosition3D(uint64_t playerId, const Vector3f& position, float yaw) const;
    void setPosition2DHandler(Position2DHandler handler);
    bool canSetPosition2D(uint64_t playerId, const Vector2f& position) const;
    /// Client collider changes require application approval before mutation or broadcast.
    /// Validate against server-owned character and match state.
    void setDimension3DHandler(Dimension3DHandler handler);
    bool canSetDimensions3D(uint64_t playerId, const Vector3f& dimensions) const;
    void setDimension2DHandler(Dimension2DHandler handler);
    bool canSetDimensions2D(uint64_t playerId, const Vector2f& dimensions) const;
    void setMovementInputHandler(MovementInputHandler handler);
    bool onMovementInput(uint64_t playerId, const Vector3f& input, float yaw);
    bool hasMovementInputHandler() const;
    /// An application callback for server-owned 2D movement. The packet's
    /// duration is ignored; the callback must queue bounded input for its tick.
    void setMovement2DInputHandler(Movement2DInputHandler handler);
    bool onMovement2DInput(uint64_t playerId, const Vector2f& input);
    bool hasMovement2DInputHandler() const;
    /// Allow legacy client-declared 2D displacement and 3D velocity/duration.
    /// Disabled by default because the library cannot check game geometry.
    void setLegacyClientMovementAllowed(bool allowed);
    bool isLegacyClientMovementAllowed() const;
    /// Required to accept 3D shots; resolves aim requests from server state.
    void setShotHandler(ShotHandler handler);
    bool hasShotHandler() const;
    std::optional<ShotHit> resolveShot(uint64_t shooterId, const Vector3f& direction,
                                       uint8_t slot, uint8_t action);
    /// Approve a 2D visual shot using server-owned weapon, ammo, and match state.
    /// The direction is normalized; a missing handler rejects the request.
    void setShot2DHandler(Shot2DHandler handler);
    bool canShoot2D(uint64_t playerId, const Vector2f& direction) const;
    /// Approve a 3D audio event using server-owned action and position state.
    /// A missing handler rejects the request.
    void setAudioEventHandler(AudioEventHandler handler);
    bool canEmitAudioEvent(uint64_t playerId, uint8_t sound, const Vector3f& position) const;
    void setSpawnHandler(SpawnHandler handler);
    void spawnPlayer(uint64_t playerId);
    /// Allow clients to create, move, and delete room objects. Disabled by
    /// default. Applications should enable this only where client-authoritative
    /// object changes are intended. Creation also requires a matching handler.
    void setClientObjectCreationAllowed(bool allowed);
    bool isClientObjectCreationAllowed() const;
    /// Approve client creation of static and moving objects. Missing handlers deny.
    /// Validate placement, dimensions, assets, and game rules against server state.
    void setObject2DCreationHandler(Object2DCreationHandler handler);
    bool canCreateObject2D(uint64_t playerId, const Vector2f& position,
                           const Vector2f& dimensions, const std::string& filepath) const;
    void setObject3DCreationHandler(Object3DCreationHandler handler);
    bool canCreateObject3D(uint64_t playerId, const Vector3f& position,
                           const Vector3f& dimensions, const std::string& filepath) const;
    /// Moving creation also needs a policy for the complete displacement and
    /// bounded destination. Configure it before accepting room traffic.
    /// Missing handlers deny moving creation before storage or broadcast.
    void setMovingObject2DCreationHandler(MovingObject2DCreationHandler handler);
    bool canCreateMovingObject2D(uint64_t playerId, const Vector2f& start,
                                 const Vector2f& dimensions, const std::string& filepath,
                                 const Vector2f& displacement, float duration,
                                 MovementType type, const Vector2f& destination) const;
    void setMovingObject3DCreationHandler(MovingObject3DCreationHandler handler);
    bool canCreateMovingObject3D(uint64_t playerId, const Vector3f& start,
                                 const Vector3f& dimensions, const std::string& filepath,
                                 const Vector3f& displacement, float duration,
                                 MovementType type, const Vector3f& destination) const;
    /// Authorize client moves and deletes of existing objects. Position is the
    /// bounded destination for moves and absent for deletes. Missing handlers deny.
    /// Handlers called during a move must not reenter room resource accessors.
    void setObject2DMutationHandler(Object2DMutationHandler handler);
    bool canMutateObject2D(uint64_t playerId, uint64_t objectId, ObjectMutation action,
                           const std::optional<Vector2f>& position) const;
    void setObject3DMutationHandler(Object3DMutationHandler handler);
    bool canMutateObject3D(uint64_t playerId, uint64_t objectId, ObjectMutation action,
                           const std::optional<Vector3f>& position) const;
    /// Allow clients to create, change, and delete game items. Disabled by default.
    /// Creation also requires a handler. A pickup handler can separately
    /// authorize pickup updates.
    void setClientGameItemCreationAllowed(bool allowed);
    bool isClientGameItemCreationAllowed() const;
    /// Approve client item creation using server-owned game state. Missing denies.
    void setGameItemCreationHandler(GameItemCreationHandler handler);
    bool canCreateGameItem(uint64_t playerId, const Vector3f& position,
                           const Vector3f& dimensions, const std::string& type,
                           const std::string& status, const std::string& filepath) const;
    /// Authorize client status changes and deletes of existing game items.
    /// Status is empty for deletes. Missing handlers deny.
    void setGameItemMutationHandler(GameItemMutationHandler handler);
    bool canMutateGameItem(uint64_t playerId, uint64_t itemId, GameItemMutation action,
                           const std::string& status) const;
    /// Allow the room creator to remove this room through a client request.
    /// Disabled by default. Server-side removal is still available directly.
    void setClientRoomRemovalAllowed(bool allowed);
    bool isClientRoomRemovalAllowed() const;
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
    mutable std::mutex m_membershipMutex;
    mutable std::mutex m_admissionMutex;
    mutable std::mutex m_broadcastMutex;
    std::deque<Broadcast> m_broadcasts;
    bool m_broadcastHistoryEnabled = false;
    std::unordered_map<uint64_t, float> m_playerHealth;
    std::unordered_map<uint64_t, uint64_t> m_playerKills;
    std::unordered_map<uint64_t, uint64_t> m_playerDeaths;
    std::unordered_map<uint64_t, std::string> m_playerTeams;
    std::vector<std::string> m_availableTeams;
    float m_defaultHealth = 100.0f;
    DeathHandler m_deathHandler;
    AdmissionHandler m_admissionHandler;
    TeamHandler m_teamHandler;
    ActionHandler m_actionHandler;
    CombatHandler m_combatHandler;
    TeamSelectionHandler m_teamSelectionHandler;
    MovementInputHandler m_movementInputHandler;
    Movement2DInputHandler m_movement2DInputHandler;
    Position3DHandler m_position3DHandler;
    Position2DHandler m_position2DHandler;
    Dimension3DHandler m_dimension3DHandler;
    Dimension2DHandler m_dimension2DHandler;
    ShotHandler m_shotHandler;
    Shot2DHandler m_shot2DHandler;
    AudioEventHandler m_audioEventHandler;
    SpawnHandler m_spawnHandler;
    GameItemPickupHandler m_gameItemPickupHandler;
    Object2DCreationHandler m_object2DCreationHandler;
    Object3DCreationHandler m_object3DCreationHandler;
    MovingObject2DCreationHandler m_movingObject2DCreationHandler;
    MovingObject3DCreationHandler m_movingObject3DCreationHandler;
    GameItemCreationHandler m_gameItemCreationHandler;
    Object2DMutationHandler m_object2DMutationHandler;
    Object3DMutationHandler m_object3DMutationHandler;
    GameItemMutationHandler m_gameItemMutationHandler;
    BroadcastHandler m_broadcastHandler;
    bool m_clientPositionAllowed = false;
    std::atomic<bool> m_legacyClientMovementAllowed{false};
    bool m_clientObjectCreationAllowed = false;
    bool m_clientGameItemCreationAllowed = false;
    bool m_clientRoomRemovalAllowed = false;

    /// Guards per-player game state (health, kills, deaths, teams). The server
    /// runs one command thread per client, so these shared maps must be
    /// synchronized to avoid races and double-counted kills.
    mutable std::mutex m_stateMutex;
    /// Serializes client team requests while policy reads current assignments.
    mutable std::mutex m_teamSelectionMutex;

    /// Atomic because movement threads read this while command threads write it.
    std::atomic<bool> m_overlappingAllowed{true};
    std::atomic<bool> m_teamDamageEnabled{false};
    mutable std::mutex m_resourceMutex;
};

} // namespace nexilis::server

#endif
