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

#include <gtest/gtest.h>

#include <chrono>
#include <csignal>
#include <thread>

#if !defined(_WIN32)
TEST(SignalHandlerTest, WaitWakesAndDispatchesCallbackOncePerRequest)
{
    using nexilis::server::SignalHandler;
    SignalHandler::install();
    SignalHandler::clearShutdownRequested();

    int callbacks = 0;
    SignalHandler::setShutdownCallback([&callbacks]()
                                       { ++callbacks; });

    ASSERT_EQ(raise(SIGINT), 0);
    SignalHandler::waitForShutdown();
    EXPECT_TRUE(SignalHandler::isShutdownRequested());
    EXPECT_EQ(callbacks, 1);

    SignalHandler::waitForShutdown();
    EXPECT_EQ(callbacks, 1);

    SignalHandler::clearShutdownRequested();
    std::thread trigger([]()
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(20));
                            raise(SIGTERM); });
    SignalHandler::waitForShutdown();
    trigger.join();
    EXPECT_EQ(callbacks, 2);

    SignalHandler::setShutdownCallback(nullptr);
    SignalHandler::clearShutdownRequested();
}
#endif

#if defined(_WIN32)
namespace
{
// On Windows the console control handler is the only thing that sets the shutdown
// flag. Raising a real Ctrl+C/Ctrl+Break event with GenerateConsoleCtrlEvent would
// also signal every other process attached to the console, so these tests drive the
// public state directly. This covers the wait/callback dispatch and the event reset
// performed by clearShutdownRequested(), which the POSIX branch does not exercise.
class WindowsSignalHandlerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nexilis::server::SignalHandler::install();
        nexilis::server::SignalHandler::setShutdownCallback(nullptr);
        nexilis::server::SignalHandler::clearShutdownRequested();
    }

    void TearDown() override
    {
        nexilis::server::SignalHandler::setShutdownCallback(nullptr);
        nexilis::server::SignalHandler::clearShutdownRequested();
    }
};
} // namespace

TEST_F(WindowsSignalHandlerTest, InstallIsIdempotent)
{
    EXPECT_NO_THROW(nexilis::server::SignalHandler::install());
    EXPECT_FALSE(nexilis::server::SignalHandler::isShutdownRequested());
}

TEST_F(WindowsSignalHandlerTest, WaitForShutdownDispatchesCallbackOnce)
{
    int callbacks = 0;
    nexilis::server::SignalHandler::setShutdownCallback([&callbacks]()
                                                        { ++callbacks; });

    nexilis::server::SignalHandler::s_shutdownRequested.store(true);

    nexilis::server::SignalHandler::waitForShutdown();
    EXPECT_TRUE(nexilis::server::SignalHandler::isShutdownRequested());
    EXPECT_EQ(callbacks, 1);

    // A second wait must not re-run the callback for the same request.
    nexilis::server::SignalHandler::waitForShutdown();
    EXPECT_EQ(callbacks, 1);
}

TEST_F(WindowsSignalHandlerTest, ClearShutdownRequestedAllowsNextCallback)
{
    int callbacks = 0;
    nexilis::server::SignalHandler::setShutdownCallback([&callbacks]()
                                                        { ++callbacks; });

    nexilis::server::SignalHandler::s_shutdownRequested.store(true);
    nexilis::server::SignalHandler::waitForShutdown();
    ASSERT_EQ(callbacks, 1);

    // Clearing resets the flag and the Windows event so a later request is
    // delivered again.
    nexilis::server::SignalHandler::clearShutdownRequested();
    ASSERT_FALSE(nexilis::server::SignalHandler::isShutdownRequested());

    nexilis::server::SignalHandler::s_shutdownRequested.store(true);
    nexilis::server::SignalHandler::waitForShutdown();
    EXPECT_EQ(callbacks, 2);
}
#endif
