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

#include <algorithm>
#include <chrono>
#include <cmath>

#include <nexilis/command_type.hh>
#include <nexilis/logger/log.hh>
#include <nexilis/room_command_type.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/command/commands.hh>
#include <nexilis/server/gameplay_limits.hh>
#include <nexilis/server/room_storage.hh>
#include <nexilis/util.hh>

namespace nexilis::server
{

CommandResult ServerImpl::room_player3d_shoot(const DefaultArgs& args)
{
    auto& user = args.getUser();
    if (user.getRoomId() == 0)
    {
        Log::error("Shoot: User not in room!");
        return CommandResult::error;
    }

    auto payload = Util::removeAmountOfBytesFromVector(args.getData(), 3);
    auto room = RoomStorage::getRoomById(user.getRoomId());
    if (!room)
    {
        Log::error("Shoot: Room not found");
        return CommandResult::failure;
    }

    uint64_t targetId = 0;
    float damage = 0.0f;
    if (room->hasShotHandler())
    {
        // Authoritative rooms accept only aim requests. Legacy target/damage
        // packets cannot bypass the server's hit and weapon checks.
        if (payload.size() != 14)
            return CommandResult::invalid_input;
        const auto direction = Util::vector3fFromFront(payload);
        if (!std::isfinite(direction.x) || !std::isfinite(direction.y) ||
            !std::isfinite(direction.z))
            return CommandResult::invalid_input;
        const auto hit = room->resolveShot(user.getId(), direction, payload[12], payload[13]);
        if (!hit)
            return CommandResult::success;
        targetId = hit->targetId;
        damage = hit->damage;
    }
    else
    {
        if (payload.size() != 12)
            return CommandResult::invalid_input;
        targetId = Util::uint64FromFront(payload);
        damage = Util::floatFromFront(Util::removeAmountOfBytesFromVector(payload, 8));
    }

    if (targetId == user.getId() || !std::isfinite(damage) || damage <= 0.0f)
        return CommandResult::invalid_input;
    const auto& limits = GameplayLimits::get();
    damage = limits.clampDamage(damage);

    bool targetFound = std::any_of(room->getClients().begin(), room->getClients().end(),
                                   [targetId](auto clientId)
                                   { return clientId == targetId; });
    if (!targetFound)
    {
        Log::warning("Shoot: Target ", targetId, " not in room");
        return CommandResult::failure;
    }

    const auto shooterTeam = room->getPlayerTeam(user.getId());
    const auto targetTeam = room->getPlayerTeam(targetId);
    if (shooterTeam.empty() || targetTeam.empty() || shooterTeam == targetTeam)
        return CommandResult::invalid_input;
    if (!room->canDamage(user.getId(), targetId))
        return CommandResult::invalid_input;
    if (room->getPlayerHealth(user.getId()) <= 0.0f)
        return CommandResult::invalid_input;

    // Shooting rate limit: a shot that arrives before the minimum
    // interval since the last accepted shot is dropped, so the
    // effective rate never exceeds the configured maximum.
    auto interval = room->hasShotHandler() ? std::chrono::duration<float>::zero()
                                           : limits.shotInterval();
    if (interval.count() > 0.0f)
    {
        auto now = std::chrono::steady_clock::now();
        if (now - user.getLastShotTime() < interval)
        {
            Log::warning("Shoot: player ", user.getId(),
                         " exceeded the shooting rate limit of ",
                         limits.getMaxShootingRate(), " shots per second");
            return CommandResult::invalid_input;
        }
        user.setLastShotTime(now);
    }

    // Only triggers a death when this hit actually brought the target from
    // alive to dead. Shots that land on an already-dead (not yet respawned)
    // player are no-ops, so a kill/death is never recorded twice.
    bool causedDeath = room->damagePlayer(targetId, damage);
    float newHealth = room->getPlayerHealth(targetId);

    Log::info("Player ", user.getId(), " shot player ", targetId,
              " for ", damage, " damage. HP: ", newHealth);

    std::map<std::string, boost::json::value> params{
            {"target_id", boost::json::value(targetId)},
            {"damage", boost::json::value(static_cast<double>(damage))},
            {"new_health", boost::json::value(static_cast<double>(newHealth))}};

    if (Command::sendRoomCommand(
                Command::createRoomCommand(
                        user.getRoomId(), user, args.getData(), params, args.getMessageId()),
                user, args.getProtocol()) == false)
        return CommandResult::failed_room_send;

    if (causedDeath)
    {
        room->onPlayerDied(user.getId(), targetId);
    }

    return CommandResult::success;
}

} // namespace nexilis::server
