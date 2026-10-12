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

#ifndef NEXILIS_SERVER_SIGNAL_HH
#define NEXILIS_SERVER_SIGNAL_HH

#include <atomic>
#include <functional>
#include <utility>

namespace nexilis::server
{

/// Signal handling utilities for server applications.
/// Provides cross-platform signal handling for graceful shutdown.
class SignalHandler
{
public:
    /// Install signal handlers for SIGINT and SIGTERM.
    /// On Windows, installs a console control handler for Ctrl+C, Ctrl+Break,
    /// console close, logoff, and system shutdown events.
    /// This function is idempotent; calling it multiple times has no additional effect.
    static void install();

    /// Check if a shutdown signal has been received.
    /// \return True if shutdown was requested, false otherwise.
    [[nodiscard]] static bool isShutdownRequested() noexcept;

    /// Block until a shutdown signal is received.
    /// Returns immediately if shutdown was already requested.
    static void waitForShutdown();

    /// Reset the shutdown requested flag.
    /// Allows the application to continue running after a signal was received.
    static void clearShutdownRequested() noexcept;

    /// Set a callback to be invoked once by waitForShutdown() for each shutdown request.
    /// The callback runs on the waiting thread, outside the OS signal/control handler.
    /// Call waitForShutdown() to dispatch the callback.
    /// \param callback Function to call on shutdown signal. Pass nullptr to remove.
    static void setShutdownCallback(std::function<void()> callback);

    /// Internal state for signal handling.
    static std::atomic<bool> s_shutdownRequested;
};

/// Free-function API for simple signal handling.
/// \see SignalHandler
inline void installSignalHandlers()
{
    SignalHandler::install();
}

/// Check if a shutdown signal has been received.
/// \return True if shutdown was requested, false otherwise.
/// \see SignalHandler::isShutdownRequested
[[nodiscard]] inline bool isShutdownRequested() noexcept
{
    return SignalHandler::isShutdownRequested();
}

/// Block until a shutdown signal is received.
/// Returns immediately if shutdown was already requested.
/// \see SignalHandler::waitForShutdown
inline void waitForShutdown()
{
    SignalHandler::waitForShutdown();
}

/// Reset the shutdown requested flag.
/// Allows the application to continue running after a signal was received.
/// \see SignalHandler::clearShutdownRequested
inline void clearShutdownRequested() noexcept
{
    SignalHandler::clearShutdownRequested();
}

/// Set a custom callback to be invoked when a shutdown signal is received.
/// The callback runs on the thread calling waitForShutdown().
/// \param callback Function to call on shutdown signal. Pass nullptr to remove.
/// \see SignalHandler::setShutdownCallback
inline void setShutdownCallback(std::function<void()> callback)
{
    SignalHandler::setShutdownCallback(std::move(callback));
}

} // namespace nexilis::server

#endif // NEXILIS_SERVER_SIGNAL_HH
