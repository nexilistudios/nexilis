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
#include <cerrno>
#include <csignal>
#include <functional>
#include <mutex>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#endif

namespace nexilis::server
{

std::atomic<bool> SignalHandler::s_shutdownRequested{false};

namespace
{
std::mutex callbackMutex;
std::function<void()> shutdownCallback;
std::atomic<bool> callbackDelivered{false};

#if defined(_WIN32)
HANDLE shutdownEvent = nullptr;
#else
int shutdownPipe[2] = {-1, -1};
#endif
} // namespace

#if defined(_WIN32)

static BOOL WINAPI consoleCtrlHandler(DWORD ctrlType)
{
    switch (ctrlType)
    {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            SignalHandler::s_shutdownRequested.store(true, std::memory_order_release);
            SetEvent(shutdownEvent);
            return TRUE;
        default:
            return FALSE;
    }
}

namespace
{
struct ConsoleRegistration
{
    ConsoleRegistration()
    {
        shutdownEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (shutdownEvent == nullptr)
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                    "CreateEventW");
        if (!SetConsoleCtrlHandler(consoleCtrlHandler, TRUE))
        {
            const auto error = GetLastError();
            CloseHandle(shutdownEvent);
            shutdownEvent = nullptr;
            throw std::system_error(static_cast<int>(error), std::system_category(),
                                    "SetConsoleCtrlHandler");
        }
    }

    ~ConsoleRegistration()
    {
        SetConsoleCtrlHandler(consoleCtrlHandler, FALSE);
        CloseHandle(shutdownEvent);
    }
};
} // namespace

void SignalHandler::install()
{
    static ConsoleRegistration registration;
    (void)registration;
}

#else

static void signalHandler(int signum)
{
    const int savedErrno = errno;
    (void)signum;
    SignalHandler::s_shutdownRequested.store(true, std::memory_order_release);
    const char wake = 1;
    const auto result = write(shutdownPipe[1], &wake, 1);
    (void)result;
    errno = savedErrno;
}

void SignalHandler::install()
{
    static std::once_flag flag;
    std::call_once(flag, []()
                   {
                       if (pipe(shutdownPipe) != 0)
                           throw std::system_error(errno, std::generic_category(), "pipe");
                       const int flags = fcntl(shutdownPipe[1], F_GETFL);
                       if (flags == -1 || fcntl(shutdownPipe[1], F_SETFL, flags | O_NONBLOCK) == -1)
                       {
                           const int error = errno;
                           close(shutdownPipe[0]);
                           close(shutdownPipe[1]);
                           shutdownPipe[0] = shutdownPipe[1] = -1;
                           throw std::system_error(error, std::generic_category(), "fcntl");
                       }
                       struct sigaction sa;
                       sa.sa_handler = signalHandler;
                       sigemptyset(&sa.sa_mask);
                       sa.sa_flags = SA_RESTART;

                       if (sigaction(SIGINT, &sa, nullptr) != 0 ||
                           sigaction(SIGTERM, &sa, nullptr) != 0)
                           throw std::system_error(errno, std::generic_category(), "sigaction");

#ifdef SIGHUP
                       if (sigaction(SIGHUP, &sa, nullptr) != 0)
                           throw std::system_error(errno, std::generic_category(), "sigaction");
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
    install();
#if defined(_WIN32)
    if (!isShutdownRequested() && WaitForSingleObject(shutdownEvent, INFINITE) != WAIT_OBJECT_0)
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                "WaitForSingleObject");
#else
    while (!isShutdownRequested())
    {
        char wake;
        if (read(shutdownPipe[0], &wake, 1) == -1 && errno != EINTR)
            throw std::system_error(errno, std::generic_category(), "read");
    }
#endif
    if (!callbackDelivered.exchange(true, std::memory_order_acq_rel))
    {
        std::function<void()> callback;
        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            callback = shutdownCallback;
        }
        if (callback)
            callback();
    }
}

void SignalHandler::clearShutdownRequested() noexcept
{
#if defined(_WIN32)
    s_shutdownRequested.store(false, std::memory_order_release);
    if (shutdownEvent != nullptr)
    {
        ResetEvent(shutdownEvent);
        if (s_shutdownRequested.load(std::memory_order_acquire))
            SetEvent(shutdownEvent);
    }
#else
    s_shutdownRequested.store(false, std::memory_order_release);
#endif
    callbackDelivered.store(false, std::memory_order_release);
}

void SignalHandler::setShutdownCallback(std::function<void()> callback)
{
    std::lock_guard<std::mutex> lock(callbackMutex);
    shutdownCallback = std::move(callback);
}

} // namespace nexilis::server
