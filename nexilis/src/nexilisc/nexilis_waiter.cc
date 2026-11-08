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

#include <nexilisc/client/client_api_c.h>
#include <nexilisc/nexilis_promise.h>
#include <nexilisc/nexilis_waiter.h>

#include <functional>
#include <future>

struct nexilis_WaiterHandle
{
    std::function<void()> callback;
    std::mutex mutex;
    bool waited = false;
};

namespace
{
std::function<void()> create_wait_callback(std::shared_ptr<nexilis::client::ClientAPI> api,
                                           std::shared_ptr<std::promise<void>> promise,
                                           nexilis_WaitType wait_type)
{
    return [api = std::move(api), promise = std::move(promise), wait_type]
    {
        if (wait_type == NEXILIS_WAIT_UNTIL_ROOMS_CREATED)
            api->waitUntilRoomsCreated (*promise)();
        else
            promise->set_exception(std::make_exception_ptr(std::runtime_error("Unknown wait type")));
    };
}
} // namespace

nexilis_WaiterHandle* nexilis_waiter_create(nexilis_ClientAPI* client_api, nexilis_PromiseHandle* promise, nexilis_WaitType wait_type)
{
    if (!client_api || !client_api->owner || !promise || !promise->promise)
        return nullptr;

    try
    {
        auto waiter = std::make_unique<nexilis_WaiterHandle>();
        waiter->callback = create_wait_callback(client_api->owner, promise->promise, wait_type);
        return waiter.release();
    }
    catch (...)
    {
        return nullptr;
    }
}

nexilis_WaiterHandle* nexilis_waiter_create_for_rooms_created(nexilis_ClientAPI* client_api, nexilis_PromiseHandle* promise)
{
    if (client_api && promise)
    {
        return nexilis_waiter_create(client_api, promise, NEXILIS_WAIT_UNTIL_ROOMS_CREATED);
    }
    else
    {
        return nullptr;
    }
}

void nexilis_waiter_wait(nexilis_WaiterHandle* waiter)
{
    if (!waiter)
        return;

    std::lock_guard<std::mutex> lock(waiter->mutex);
    if (waiter->waited)
        return;

    if (waiter->callback)
    {
        try
        {
            waiter->callback();
            waiter->waited = true;
        }
        catch (...)
        {
            // Handle exceptions
        }
    }
}

void nexilis_waiter_destroy(nexilis_WaiterHandle* waiter)
{
    if (waiter)
    {
        {
            std::lock_guard<std::mutex> lock(waiter->mutex);
            waiter->callback = {};
        }
        delete waiter;
    }
}

bool nexilis_waiter_is_valid(const nexilis_WaiterHandle* waiter)
{
    return waiter != nullptr && waiter->callback != nullptr;
}
