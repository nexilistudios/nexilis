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

#include <nexilis/client/client_session.hh>
#include <nexilisc/client/client_session_c.h>

#include <nexilis/nexilis_constants.hh>

#include <float.h>
#include <math.h>

nexilis_ClientSession* nexilis_client_session_create(uint64_t id, nexilis_ClientAPI* client_api)
{
    auto session = new nexilis::client::ClientSession(id, client_api->api);
    return new nexilis_ClientSession{session};
}

void nexilis_client_session_destroy(nexilis_ClientSession* client)
{
    if (client)
    {
        delete static_cast<nexilis::client::ClientSession*>(client->client);
        delete client;
    }
}

void nexilis_client_session_release_borrowed(nexilis_ClientSession* session)
{
    delete session;
}

void nexilis_client_session_free_wrapper(nexilis_ClientSession* session)
{
    // Free only the wrapper struct. The underlying ClientSession is
    // owned by the ClientAPI and must not be destroyed here.
    delete session;
}

uint64_t nexilis_client_session_get_id(nexilis_ClientSession* session)
{
    if (session && session->client)
    {
        return session->client->getId();
    }
    return 0;
}

void nexilis_client_session_set_position_3D(nexilis_ClientSession* client, float x, float y, float z)
{
    if (client && client->client)
    {
        client->client->setPosition3D(x, y, z);
    }
}

nexilis_Vector3f* nexilis_client_session_get_position_3D(nexilis_ClientSession* client)
{
    if (!client || !client->client)
    {
        nexilis::FileLog::critical("nexilis_client_session_get_position_3D: client or client->client is null");
        return nullptr;
    }
    try
    {
        auto* vector = new nexilis_Vector3f;
        vector->vec = new nexilis::Vector3f(client->client->getPosition3D());
        return vector;
    }
    catch (...)
    {
        nexilis::FileLog::critical("Problem with nexilis_client_session_get_position_3D");
        return nullptr;
    }
}

nexilis::Vector3f get_verified_position(nexilis_ClientSession* client)
{
    auto pos = client->client->getPosition3D();

    uint32_t x_bits, y_bits, z_bits;
    std::memcpy(&x_bits, &pos.x, sizeof(pos.x));
    std::memcpy(&y_bits, &pos.y, sizeof(pos.y));
    std::memcpy(&z_bits, &pos.z, sizeof(pos.z));

    // Common bad patterns.
    if (x_bits == 0xCDCDCDCD || y_bits == 0xCDCDCDCD || z_bits == 0xCDCDCDCD)
    {
        nexilis::FileLog::critical("get_verified_position: Uninitialized memory detected");
    }
    if (x_bits == 0xFEEEFEEE || y_bits == 0xFEEEFEEE || z_bits == 0xFEEEFEEE)
    {
        nexilis::FileLog::critical("get_verified_position: Freed memory detected");
    }
    return pos;
}

bool validate_position(const nexilis::Vector3f& pos)
{
    // Check for NaN/infinity.
    if (!std::isfinite(pos.x))
        return false;
    if (!std::isfinite(pos.y))
        return false;
    if (!std::isfinite(pos.z))
        return false;

    // Check for denormal numbers.
    if (std::fpclassify(pos.x) == FP_SUBNORMAL)
        return false;
    if (std::fpclassify(pos.y) == FP_SUBNORMAL)
        return false;
    if (std::fpclassify(pos.z) == FP_SUBNORMAL)
        return false;

    // Game world limits.
    if (fabs(pos.x) > nexilis::NEXILIS_MAX_POSITION)
    {
        nexilis::FileLog::critical("validate_position: x size exceeded");
        return false;
    }
    if (fabs(pos.y) > nexilis::NEXILIS_MAX_POSITION)
    {
        nexilis::FileLog::critical("validate_position: y size exceeded");
        return false;
    }
    if (fabs(pos.z) > nexilis::NEXILIS_MAX_POSITION)
    {
        nexilis::FileLog::critical("validate_position: z size exceeded");
        return false;
    }
    return true;
}

bool nexilis_client_session_get_position_3D_values(nexilis_ClientSession* client, float* x, float* y, float* z)
{
    if (!client || !client->client || !x || !y || !z)
    {
        return false;
    }

    try
    {
        auto pos = get_verified_position(client);

        if (!validate_position(pos))
        {
            nexilis::FileLog::critical("nexilis_client_session_get_position_3D_values: Invalid position values");
            return false;
        }

        *x = pos.x;
        *y = pos.y;
        *z = pos.z;
        return true;
    }
    catch (...)
    {
        nexilis::FileLog::critical("nexilis_client_session_get_position_3D_values: Failed to get values");
        return false;
    }
}

nexilis_ClientSession* nexilis_client_session_move(nexilis_ClientSession* other)
{
    if (!other)
    {
        return nullptr;
    }

    auto moved_session = new nexilis::client::ClientSession(
            std::move(*static_cast<nexilis::client::ClientSession*>(other->client)));
    return new nexilis_ClientSession{moved_session};
}

void nexilis_client_session_move_assign(nexilis_ClientSession* dest, nexilis_ClientSession* src)
{
    if (!dest || !src)
        return;

    *static_cast<nexilis::client::ClientSession*>(dest->client) =
            std::move(*static_cast<nexilis::client::ClientSession*>(src->client));
}

bool nexilis_client_session_equal(const nexilis_ClientSession* lhs, const nexilis_ClientSession* rhs)
{
    if (!lhs || !rhs)
        return false;

    return *static_cast<const nexilis::client::ClientSession*>(lhs->client) ==
           *static_cast<const nexilis::client::ClientSession*>(rhs->client);
}

bool nexilis_client_session_not_equal(const nexilis_ClientSession* lhs, const nexilis_ClientSession* rhs)
{
    return !nexilis_client_session_equal(lhs, rhs);
}

void nexilis_client_session_set_username(nexilis_ClientSession* client, const char* new_username)
{
    if (client && new_username)
    {
        static_cast<nexilis::client::ClientSession*>(client->client)->setUsername(new_username);
    }
}
