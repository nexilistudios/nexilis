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

#include <gtest/gtest.h>

#include <nexilis/client/client_api.hh>
#include <nexilis/client/client_config.hh>
#include <nexilis/client/protocol/nxboost/tcp_client.hh>
#include <nexilis/crypto.hh>
#include <nexilis/server/server_config.hh>
#include <nexilis/tls.hh>

#include <stdexcept>
#include <string>

using namespace nexilis;

namespace
{

TEST(TLS, ClientConfigOnByDefault)
{
    client::ClientConfig config;
    EXPECT_TRUE(config.isTlsEnabled());
}

TEST(TLS, ClientConfigToggleFlipsState)
{
    client::ClientConfig config;
    config.setTls(true);
    EXPECT_TRUE(config.isTlsEnabled());
    config.setTls(false);
    EXPECT_FALSE(config.isTlsEnabled());
}

TEST(TLS, ServerConfigOnByDefault)
{
    server::ServerConfig config;
    EXPECT_TRUE(config.isTlsEnabled());
}

TEST(TLS, ServerConfigToggleFlipsState)
{
    server::ServerConfig config;
    config.setTls(true);
    EXPECT_TRUE(config.isTlsEnabled());
    config.setTls(false);
    EXPECT_FALSE(config.isTlsEnabled());
}

TEST(TLS, DerivePskIsDeterministic)
{
    const auto first = tls::derivePsk("salasana");
    const auto second = tls::derivePsk("salasana");
    EXPECT_EQ(first, second);
    EXPECT_FALSE(first.empty());
}

TEST(TLS, DerivePskKeyLength)
{
    EXPECT_EQ(tls::derivePsk("salasana").size(), tls::kPskLength);
    EXPECT_EQ(tls::kPskLength, 32u);
}

TEST(TLS, DerivePskDiffersBetweenPasswords)
{
    EXPECT_NE(tls::derivePsk("salasana"), tls::derivePsk("salasama"));
}

TEST(TLS, DerivePskDiffersFromMessageKey)
{
    const auto psk = tls::derivePsk("salasana");
    const auto messageKey = crypto::deriveMessageKey("salasana", 42);
    EXPECT_NE(psk, messageKey);
}

TEST(TLS, CreatePskContextFailsWithoutPassword)
{
    EXPECT_EQ(tls::createPskContext("", false), nullptr);
    EXPECT_EQ(tls::createPskContext("", true), nullptr);
}

TEST(TLS, CreatePskContextServer)
{
    auto ctx = tls::createPskContext("salasana", true);
    EXPECT_NE(ctx, nullptr);
}

TEST(TLS, CreatePskContextClient)
{
    auto ctx = tls::createPskContext("salasana", false);
    EXPECT_NE(ctx, nullptr);
}

TEST(TLS, BoostTcpClientRequiresPasswordByDefault)
{
    client::ClientConfig config;
    client::ClientAPI api(config);
    EXPECT_THROW(client::nxboost::TCPClient tcpClient(api), std::runtime_error);
}

} // namespace
