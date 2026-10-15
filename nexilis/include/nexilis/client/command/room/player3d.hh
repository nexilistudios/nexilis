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

#ifndef NEXILIS_CLIENT_COMMAND_ROOM_PLAYER3D_HH
#define NEXILIS_CLIENT_COMMAND_ROOM_PLAYER3D_HH

#include <nexilis/client/base_api_command.hh>
#include <nexilis/client/client_api.hh>

namespace nexilis::client
{

class RoomPlayer3DCommand : public BaseAPICommand
{
public:
    RoomPlayer3DCommand(std::string action, uint64_t client_id, float x, float y, float z,
                        uint64_t target_id = 0, float damage = 0.0f, float new_health = 0.0f,
                        float yaw = 0.0f)
        : m_action(action), m_client_id(client_id), m_x(x), m_y(y), m_z(z),
          m_target_id(target_id), m_damage(damage), m_new_health(new_health), m_yaw(yaw)
    {
    }

    ReadResult execute(ClientAPI&, ClientAPI::ClientAPIData& data) override
    {
        auto& mtx = data.getRoomsMutex();
        std::lock_guard<std::mutex> lock(*mtx);

        auto& rooms = data.getCurrentlyActiveRooms();

        if (m_action == "position")
        {
            for (auto&& room = rooms.begin(); room != rooms.end(); room++)
            {
                for (auto& client : room->getClients())
                {
                    if (client.getId() == m_client_id)
                    {
                        client.getObject3D().setPosition(Vector3(m_x, m_y, m_z));
                        client.getObject3D().setYaw(m_yaw);
                        return ReadResult::success;
                    }
                }
            }
            return ReadResult::command_execution;
        }
        else if (m_action == "dimensions")
        {
            for (auto&& room = rooms.begin(); room != rooms.end(); room++)
            {
                for (auto& client : room->getClients())
                {
                    if (client.getId() == m_client_id)
                    {
                        client.getObject3D().setDimensions(Vector3(m_x, m_y, m_z));
                        return ReadResult::success;
                    }
                }
            }
            return ReadResult::command_execution;
        }
        else if (m_action == "movement")
        {
            for (auto&& room = rooms.begin(); room != rooms.end(); room++)
            {
                for (auto& client : room->getClients())
                {
                    if (client.getId() == m_client_id)
                    {
                        client.getObject3D().setPosition(Vector3(m_x, m_y, m_z));
                        client.getObject3D().setYaw(m_yaw);
                        return ReadResult::success;
                    }
                }
            }
            return ReadResult::command_execution;
        }
        else if (m_action == "shoot")
        {
            ClientAPI::DamageEvent event;
            event.shooter_id = m_client_id;
            event.target_id = m_target_id;
            event.damage = m_damage;
            event.new_health = m_new_health;
            data.pushDamageEvent(std::move(event));
            return ReadResult::success;
        }
        else if (m_action == "respawn")
        {
            ClientAPI::RespawnEvent event;
            event.target_id = m_target_id;
            data.pushRespawnEvent(std::move(event));
            return ReadResult::success;
        }
        return ReadResult::not_found;
    }

private:
    std::string m_action;
    uint64_t m_client_id;
    float m_x;
    float m_y;
    float m_z;
    uint64_t m_target_id;
    float m_damage;
    float m_new_health;
    float m_yaw;
};

/// Handles server-relayed positional audio events. The server acts as a relay:
/// it forwards a player's sound event (sound type + world position) to every
/// other client in the room, and each client spatializes the sound itself.
class RoomAudioEventCommand : public BaseAPICommand
{
public:
    RoomAudioEventCommand(uint64_t client_id, uint8_t sound, float x, float y, float z)
        : m_client_id(client_id), m_sound(sound), m_x(x), m_y(y), m_z(z)
    {
    }

    ReadResult execute(ClientAPI&, ClientAPI::ClientAPIData& data) override
    {
        ClientAPI::AudioEvent event;
        event.client_id = m_client_id;
        event.sound = m_sound;
        event.x = m_x;
        event.y = m_y;
        event.z = m_z;
        data.pushAudioEvent(std::move(event));
        return ReadResult::success;
    }

private:
    uint64_t m_client_id;
    uint8_t m_sound;
    float m_x;
    float m_y;
    float m_z;
};

class RoomAnimationEventCommand : public BaseAPICommand
{
public:
    RoomAnimationEventCommand(uint64_t room_id, uint64_t client_id, std::string state,
                              std::string weapon, float duration)
        : m_room_id(room_id), m_client_id(client_id), m_state(std::move(state)),
          m_weapon(std::move(weapon)), m_duration(duration)
    {
    }

    ReadResult execute(ClientAPI&, ClientAPI::ClientAPIData& data) override
    {
        data.pushAnimationEvent({m_room_id, m_client_id, m_state, m_weapon, m_duration});
        return ReadResult::success;
    }

private:
    uint64_t m_room_id;
    uint64_t m_client_id;
    std::string m_state;
    std::string m_weapon;
    float m_duration;
};

} // namespace nexilis::client

#endif
