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

#include <nexilis/nx_protocol.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/gameplay_limits.hh>
#include <nexilis/server/movement.hh>
#include <nexilis/server/room_storage.hh>
#include <nexilis/server/validation.hh>

namespace nexilis::server
{

ServerConfig* Movement::m_settings = nullptr;

bool Movement::isInitialized()
{
    return m_settings;
}

void Movement::_initialize(ServerConfig& settings)
{
    m_settings = &settings;
}

double Movement::easing(double progress, double totalDistance)
{
    double easedValue = progress * progress;
    double messageValue = totalDistance * easedValue;
    return messageValue;
}

double Movement::linear(double progress, double totalDistance)
{
    return totalDistance * progress;
}

void Movement::object2D(std::unique_ptr<Movement2D> movement, User& user, NXProtocol& protocol)
{
    if (!movement)
        return;
    auto room = RoomStorage::getRoomById(user.getRoomId());
    if (!room)
        return;
    const auto amount = movement->getAmount();
    const auto func = movement->getMovementFunc();
    const auto& limits = GameplayLimits::get();
    // Complete the displacement during dispatch; no task retains the user,
    // protocol, room, or object after the command returns.
    Vector2f position;
    const bool found = room->withObject2D(movement->getObjectId(), [&](Object2D& object)
                                          {
        const auto start = object.getPosition();
        position = Vector2f(limits.clampHorizontal(start.x + func(1.0, amount.x)),
                            limits.clampHorizontal(start.y + func(1.0, amount.y)));
        if (validation::isFinite(position))
            object.setPosition(position); });
    if (!found || !validation::isFinite(position))
        return;
    std::map<std::string, boost::json::value> params{
            {"createMovingType", boost::json::value("update")},
            {"id", boost::json::value(movement->getObjectId())},
            {"x", boost::json::value(position.x)},
            {"y", boost::json::value(position.y)}};
    auto command = Command::createRoomCommand(user.getRoomId(), user, movement->getMessageData(), params, movement->getMessageId());
    Command::sendRoomCommand(command, user, protocol);
}

void Movement::object3D(std::unique_ptr<Movement3D> movement, User& user, NXProtocol& protocol)
{
    if (!movement)
        return;
    auto room = RoomStorage::getRoomById(user.getRoomId());
    if (!room)
        return;
    const auto amount = movement->getAmount();
    const auto func = movement->getMovementFunc();
    const auto& limits = GameplayLimits::get();
    Vector3f position;
    const bool found = room->withObject3D(movement->getObjectId(), [&](Object3D& object)
                                          {
        const auto start = object.getPosition();
        position = Vector3f(limits.clampHorizontal(start.x + func(1.0, amount.x)),
                            limits.clampHeight(start.y + func(1.0, amount.y)),
                            limits.clampHorizontal(start.z + func(1.0, amount.z)));
        if (validation::isFinite(position))
            object.setPosition(position); });
    if (!found || !validation::isFinite(position))
        return;
    std::map<std::string, boost::json::value> params{
            {"createMovingType", boost::json::value("update")},
            {"id", boost::json::value(movement->getObjectId())},
            {"x", boost::json::value(position.x)},
            {"y", boost::json::value(position.y)},
            {"z", boost::json::value(position.z)}};
    auto command = Command::createRoomCommand(user.getRoomId(), user, movement->getMessageData(), params, movement->getMessageId());
    Command::sendRoomCommand(command, user, protocol);
}

} // namespace nexilis::server
