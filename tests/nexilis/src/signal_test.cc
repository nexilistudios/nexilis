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
