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

#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/command/commands.hh>
#include <nexilis/server/movement.hh>
#include <nexilis/server/room_storage.hh>

#include <cmath>

namespace nexilis::server
{

CommandResult ServerImpl::room_player3d_movement(const DefaultArgs& args)
{
    auto& user = args.getUser();
    auto& protocol = args.getProtocol();
    auto messageId = args.getMessageId();
    auto data = args.getData();

    auto payload = Util::removeAmountOfBytesFromVector(data, 3);
    if (payload.size() != 16 && payload.size() != 20)
        return CommandResult::invalid_input;
    auto vec_x = Util::floatFromFront(payload);
    auto vec_y = Util::floatFromFront(Util::removeAmountOfBytesFromVector(payload, 4));
    auto vec_z = Util::floatFromFront(Util::removeAmountOfBytesFromVector(payload, 8));
    auto delta = Util::floatFromFront(Util::removeAmountOfBytesFromVector(payload, 12));
    const float yaw = payload.size() == 20
                              ? Util::floatFromFront(Util::removeAmountOfBytesFromVector(payload, 16))
                              : 0.0f;
    if (!std::isfinite(vec_x) || !std::isfinite(vec_y) || !std::isfinite(vec_z) ||
        !std::isfinite(delta) || !std::isfinite(yaw))
        return CommandResult::invalid_input;
    auto movement_vector = Vector3f(vec_x, vec_y, vec_z);
    auto mtx = std::make_shared<std::mutex>();

    // clang-format off
    std::thread([mtx, movement_vector, &user, data, &protocol, messageId, delta, yaw]()
    {
        try
        {
            auto* room = RoomStorage::getRoomById(user.getRoomId());
            if (!room)
            {
                Log::warning("room_player3d_movement: room not found for user ", user.getRoomId());
                return;
            }

            // The client sends a movement vector (velocity) together with the
            // frame duration. Displacement for this packet is velocity * delta.
            // Applying it exactly once (instead of via a tick-rate easing loop)
            // guarantees the movement is applied even when delta is shorter than
            // a single server tick.
            if (delta <= 0.0f)
            {
                return;
            }

            auto current_pos = user.getObject3D().getPosition();
            auto new_pos = Vector3f(current_pos.x + movement_vector.x * delta,
                                    current_pos.y + movement_vector.y * delta,
                                    current_pos.z + movement_vector.z * delta);

            // Overlap validation: when the room forbids clients from sharing
            // positions, drop this move if the new position would intersect
            // another client. The position is not applied nor broadcast.
            if (!room->isOverlappingAllowed())
            {
                auto dimensions = user.getObject3D().getDimensions();
                for (auto clientId : room->getClients())
                {
                    if (clientId == user.getId())
                    {
                        continue;
                    }
                    auto* other = ClientStorage::getClientById(clientId);
                    if (!other || other->getRoomId() != room->getId())
                    {
                        continue;
                    }
                    auto other_pos = other->getObject3D().getPosition();
                    auto other_dimensions = other->getObject3D().getDimensions();

                    bool intersects =
                            new_pos.x - dimensions.x / 2 < other_pos.x + other_dimensions.x / 2 &&
                            new_pos.x + dimensions.x / 2 > other_pos.x - other_dimensions.x / 2 &&
                            new_pos.y - dimensions.y / 2 < other_pos.y + other_dimensions.y / 2 &&
                            new_pos.y + dimensions.y / 2 > other_pos.y - other_dimensions.y / 2 &&
                            new_pos.z - dimensions.z / 2 < other_pos.z + other_dimensions.z / 2 &&
                            new_pos.z + dimensions.z / 2 > other_pos.z - other_dimensions.z / 2;
                    if (intersects)
                    {
                        return;
                    }
                }
            }

            {
                std::lock_guard<std::mutex> lock(*mtx);
                user.getObject3D().setPosition(new_pos);
                user.getObject3D().setYaw(yaw);
                std::map<std::string, boost::json::value> params
                {
                    {"x", boost::json::value(new_pos.x)},
                    {"y", boost::json::value(new_pos.y)},
                    {"z", boost::json::value(new_pos.z)},
                    {"yaw", boost::json::value(yaw)},
                };
                auto room_command = Command::createRoomCommand(user.getRoomId(), user, data, params, messageId);
                Command::sendRoomCommand(room_command, user, protocol);
            }
        }
        catch (const std::exception& e)
        {
            Log::error("Error with 3D movement thread: ", e.what());
        }
        catch (...)
        {
            Log::error("Other error with 3D movement thread");
        }
    })
    .detach();
    // clang-format on
    return CommandResult::success;
}

} // namespace nexilis::server
