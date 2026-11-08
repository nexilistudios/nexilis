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

#include <boost/asio/write.hpp>
#include <chrono>
#include <future>
#include <nexilis/boost_tcp/socket.hh>
#include <nexilis/client/client_api.hh>
#include <nexilis/client/client_config.hh>
#include <nexilis/client/protocol/nxboost/tcp_client.hh>
#include <nexilis/crypto.hh>
#include <nexilis/nexilis_constants.hh>
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

// Use an ephemeral loopback port and no running shared io_context: the Socket
// must drive its own deadline and cancellation handlers to completion.
class FramedSocketTest : public ::testing::Test
{
protected:
    boost::asio::io_context io;
    boost::asio::ip::tcp::acceptor acceptor{io, {boost::asio::ip::tcp::v4(), 0}};
    boost::asio::ip::tcp::socket peer{io};
    boost_tcp::Socket receiver{io};

    void SetUp() override
    {
        peer.connect({boost::asio::ip::address_v4::loopback(), acceptor.local_endpoint().port()});
        receiver.assign(acceptor.accept());
    }

    void write(const nx_data& data)
    {
        boost::asio::write(peer, boost::asio::buffer(data));
    }
};

TEST_F(FramedSocketTest, AcceptsBoundaryAndCoalescedFrames)
{
    const nx_data payload(NEXILIS_BUFFER, 42);
    auto bytes = client::ClientProtocol::frame(payload);
    const auto empty = client::ClientProtocol::frame({});
    bytes.insert(bytes.end(), empty.begin(), empty.end());
    write(bytes);
    nx_data received;
    ASSERT_TRUE(receiver.receive(received));
    EXPECT_EQ(received, payload);
    ASSERT_TRUE(receiver.receive(received));
    EXPECT_TRUE(received.empty());
    EXPECT_TRUE(receiver.isOpen());
}

TEST_F(FramedSocketTest, RejectsOversizedLengthWithoutReadingBody)
{
    write({0, 0, 4, 1}); // 1025 bytes; no body is sent.
    nx_data received;
    EXPECT_FALSE(receiver.receive(received));
    EXPECT_FALSE(receiver.isOpen());
}

TEST_F(FramedSocketTest, RejectsMaximumUint32Length)
{
    write({255, 255, 255, 255});
    nx_data received;
    EXPECT_FALSE(receiver.receive(received));
    EXPECT_FALSE(receiver.isOpen());
}

TEST_F(FramedSocketTest, IdlePollDoesNotCloseConnection)
{
    nx_data received;
    EXPECT_FALSE(receiver.receive(received, 10));
    EXPECT_TRUE(receiver.isOpen());
}

TEST_F(FramedSocketTest, IncompletePrefixTimesOut)
{
    write({0});
    nx_data received;
    const auto start = std::chrono::steady_clock::now();
    EXPECT_FALSE(receiver.receive(received));
    EXPECT_FALSE(receiver.isOpen());
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(8));
}

TEST_F(FramedSocketTest, IncompletePayloadTimesOut)
{
    write({0, 0, 0, 10, 42});
    nx_data received;
    const auto start = std::chrono::steady_clock::now();
    EXPECT_FALSE(receiver.receive(received));
    EXPECT_FALSE(receiver.isOpen());
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(8));
}

TEST_F(FramedSocketTest, TlsAcceptsValidFrameThenRejectsOversizedLength)
{
    receiver.enableTls(tls::createPskContext("secret", true));
    // enableTls must precede assign so the stream uses that context.
    receiver.close();
    peer.close();
    peer.connect({boost::asio::ip::address_v4::loopback(), acceptor.local_endpoint().port()});
    receiver.assign(acceptor.accept());
    auto sender = std::async(std::launch::async, [&]
                             {
        auto context = tls::createPskContext("secret", false);
        boost::asio::ssl::stream<boost::asio::ip::tcp::socket> stream(std::move(peer), *context);
        stream.handshake(boost::asio::ssl::stream_base::client);
        auto bytes = client::ClientProtocol::frame(nx_data{42});
        bytes.insert(bytes.end(), {255, 255, 255, 255});
        boost::asio::write(stream, boost::asio::buffer(bytes)); });
    nx_data received;
    ASSERT_TRUE(receiver.receive(received));
    EXPECT_EQ(received, nx_data{42});
    EXPECT_FALSE(receiver.receive(received));
    EXPECT_FALSE(receiver.isOpen());
    sender.get();
}

TEST_F(FramedSocketTest, StalledTlsHandshakeTimesOut)
{
    receiver.enableTls(tls::createPskContext("secret", true));
    receiver.close();
    peer.close();
    peer.connect({boost::asio::ip::address_v4::loopback(), acceptor.local_endpoint().port()});
    receiver.assign(acceptor.accept());
    nx_data received;
    const auto start = std::chrono::steady_clock::now();
    EXPECT_FALSE(receiver.receive(received));
    EXPECT_FALSE(receiver.isOpen());
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(8));
}

} // namespace
