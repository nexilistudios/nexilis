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

#include <nexilis/server/signal.hh>

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <functional>
#include <mutex>

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#endif

namespace nexilis::server
{

std::atomic<bool> SignalHandler::s_shutdownRequested{false};
std::function<void()> SignalHandler::s_shutdownCallback{};

#if defined(_WIN32)

static BOOL WINAPI consoleCtrlHandler(DWORD ctrlType)
{
    switch (ctrlType)
    {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            SignalHandler::s_shutdownRequested.store(true, std::memory_order_release);
            if (SignalHandler::s_shutdownCallback)
            {
                SignalHandler::s_shutdownCallback();
            }
            return TRUE;
        default:
            return FALSE;
    }
}

void SignalHandler::install()
{
    static std::once_flag flag;
    std::call_once(flag, []()
                   { SetConsoleCtrlHandler(consoleCtrlHandler, TRUE); });
}

#else

static void signalHandler(int signum)
{
    (void)signum;
    SignalHandler::s_shutdownRequested.store(true, std::memory_order_release);
    if (SignalHandler::s_shutdownCallback)
    {
        SignalHandler::s_shutdownCallback();
    }
}

void SignalHandler::install()
{
    static std::once_flag flag;
    std::call_once(flag, []()
                   {
                       struct sigaction sa{};
                       sa.sa_handler = signalHandler;
                       sigemptyset(&sa.sa_mask);
                       sa.sa_flags = SA_RESTART;

                       sigaction(SIGINT, &sa, nullptr);
                       sigaction(SIGTERM, &sa, nullptr);

#ifdef SIGHUP
                       sigaction(SIGHUP, &sa, nullptr);
#endif
                   });
}

#endif

bool SignalHandler::isShutdownRequested() noexcept
{
    return s_shutdownRequested.load(std::memory_order_acquire);
}

void SignalHandler::waitForShutdown()
{
    std::mutex mtx;
    std::condition_variable cv;
    {
        std::unique_lock<std::mutex> lock(mtx);

        cv.wait(lock, []()
                { return SignalHandler::isShutdownRequested(); });
    }
}

void SignalHandler::clearShutdownRequested() noexcept
{
    s_shutdownRequested.store(false, std::memory_order_release);
}

void SignalHandler::setShutdownCallback(std::function<void()> callback)
{
    s_shutdownCallback = std::move(callback);
}

} // namespace nexilis::server
