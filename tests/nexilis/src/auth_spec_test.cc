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

#include <nexilis/auth_config.hh>
#include <nexilis/command_type.hh>
#include <nexilis/message_auth.hh>
#include <nexilis/nx_protocol.hh>
#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/message/auth_message.hh>
#include <nexilis/server/message/base_message.hh>
#include <nexilis/server/message/error_message.hh>
#include <nexilis/server/message/message.hh>
#include <nexilis/server/message/message_handler.hh>
#include <nexilis/server/server_config.hh>
#include <nexilis/server/user.hh>
#include <nexilis/util.hh>

#include <algorithm>
#include <string>

using namespace nexilis;

namespace
{

TEST(ConstantTimeEquals, MatchesIdenticalStrings)
{
    EXPECT_TRUE(Util::constantTimeEquals("secret", "secret"));
    EXPECT_TRUE(Util::constantTimeEquals("", ""));
}

TEST(ConstantTimeEquals, RejectsSameLengthMismatch)
{
    EXPECT_FALSE(Util::constantTimeEquals("secret", "secrex"));
}

TEST(ConstantTimeEquals, RejectsShorterString)
{
    EXPECT_FALSE(Util::constantTimeEquals("secret", "sec"));
}

TEST(ConstantTimeEquals, RejectsLongerString)
{
    EXPECT_FALSE(Util::constantTimeEquals("secret", "secretpassword"));
}

TEST(ConstantTimeEquals, RejectsLengthMismatchEvenWhenPrefixMatches)
{
    EXPECT_FALSE(Util::constantTimeEquals("secret", "secretX"));
    EXPECT_FALSE(Util::constantTimeEquals("secretX", "secret"));
}

TEST(ConstantTimeEquals, RejectsEmptyVsNonEmpty)
{
    EXPECT_FALSE(Util::constantTimeEquals("", "a"));
    EXPECT_FALSE(Util::constantTimeEquals("a", ""));
}

TEST(AuthConfig, IsPasswordMatches)
{
    AuthConfig config(AuthenticationMode::password_protected, "secret");
    EXPECT_TRUE(config.isPassword("secret"));
    EXPECT_FALSE(config.isPassword("sec"));
    EXPECT_FALSE(config.isPassword("secretpassword"));
    EXPECT_FALSE(config.isPassword("secrex"));
}

TEST(ServerConfig, IsPassphraseMatches)
{
    server::ServerConfig config;
    config.setPassphrase("secret");
    EXPECT_TRUE(config.isPassphrase("secret"));
    EXPECT_FALSE(config.isPassphrase("sec"));
    EXPECT_FALSE(config.isPassphrase("secretpassword"));
    EXPECT_FALSE(config.isPassphrase("secrex"));
}

TEST(ServerConfig, IsRootPasswordMatches)
{
    server::ServerConfig config;
    config.setRootPassword("rootsecret");
    EXPECT_TRUE(config.isRootPassword("rootsecret"));
    EXPECT_FALSE(config.isRootPassword("rootsec"));
    EXPECT_FALSE(config.isRootPassword("rootsecretpassword"));
    EXPECT_FALSE(config.isRootPassword("rootsecr"));
}

TEST(MessageAuthentication, RejectsTamperingAndReplaysBeforeDispatch)
{
    server::ClientStorage::clear();
    struct ClearClients
    {
        ~ClearClients()
        {
            server::ClientStorage::clear();
        }
    } cleanup;
    auto user = std::make_unique<server::User>(42, "127.0.0.1");
    const auto key = user->getUdpToken();
    server::ClientStorage::add(std::move(user));

    server::ServerConfig config;
    config.setMode(AuthenticationMode::skip);
    server::MessageHandler handler;
    auto request = [](uint64_t id)
    {
        auto bytes = Util::convertToByteVector(uint64_t{42});
        auto messageId = Util::convertToByteVector(id);
        bytes.insert(bytes.end(), messageId.begin(), messageId.end());
        bytes.push_back(static_cast<uint8_t>(CommandType::getting));
        bytes.push_back(0);
        return bytes;
    };

    auto first = request(1);
    auto signedFirst = message_auth::sign(first, key);
    ASSERT_FALSE(signedFirst.empty());
    ASSERT_EQ(signedFirst.size(), first.size() + message_auth::tag_size);
    EXPECT_TRUE(std::equal(first.begin(), first.begin() + message_auth::request_header_size,
                           signedFirst.begin()));
    EXPECT_TRUE(std::equal(first.begin() + message_auth::request_header_size, first.end(),
                           signedFirst.begin() + message_auth::request_header_size +
                                   message_auth::tag_size));
    EXPECT_EQ(handler.readMessage("127.0.0.1", first, &config), nullptr);
    EXPECT_EQ(handler.readMessage("127.0.0.1", message_auth::sign(first, std::string(32, '0')), &config), nullptr);
    auto tamperedTag = signedFirst;
    tamperedTag[message_auth::request_header_size] ^= 1;
    EXPECT_EQ(handler.readMessage("127.0.0.1", tamperedTag, &config), nullptr);
    auto tamperedCommand = signedFirst;
    tamperedCommand[message_auth::request_header_size + message_auth::tag_size] ^= 1;
    EXPECT_EQ(handler.readMessage("127.0.0.1", tamperedCommand, &config), nullptr);

    ASSERT_NE(handler.readMessage("127.0.0.1", signedFirst, &config), nullptr);
    EXPECT_EQ(handler.readMessage("127.0.0.1", signedFirst, &config), nullptr);
    ASSERT_NE(handler.readMessage("127.0.0.1", message_auth::sign(request(3), key), &config), nullptr);
    ASSERT_NE(handler.readMessage("127.0.0.1", message_auth::sign(request(2), key), &config), nullptr);
    EXPECT_EQ(handler.readMessage("127.0.0.1", message_auth::sign(request(2), key), &config), nullptr);
    ASSERT_NE(handler.readMessage("127.0.0.1", message_auth::sign(request(2000), key), &config), nullptr);
    EXPECT_EQ(handler.readMessage("127.0.0.1", signedFirst, &config), nullptr);
}

TEST(MessageAuthentication, KeepsVariableLengthCommandAtEnd)
{
    const std::string key(32, 'a');
    nx_data request(16, 0);
    request.insert(request.end(), {static_cast<uint8_t>(CommandType::setting), 0, 'a', 'b', 'c'});

    const auto signedRequest = message_auth::sign(request, key);
    ASSERT_EQ(signedRequest.size(), request.size() + message_auth::tag_size);
    EXPECT_EQ(nx_data(signedRequest.end() - 5, signedRequest.end()),
              nx_data({static_cast<uint8_t>(CommandType::setting), 0, 'a', 'b', 'c'}));

    nx_data verified;
    EXPECT_TRUE(message_auth::verify(signedRequest, key, verified));
    EXPECT_EQ(verified, request);
}

class AuthTestProtocol : public NXProtocol
{
public:
    explicit AuthTestProtocol(Type type)
        : m_type(type)
    {
    }

    void start() override
    {
    }
    void stop() override
    {
    }
    Type getType() override
    {
        return m_type;
    }

private:
    Type m_type;
};

nx_data authCommand(uint8_t action, const std::string& password)
{
    nx_data command{static_cast<uint8_t>(CommandType::authentication), action};
    command.insert(command.end(), password.begin(), password.end());
    return command;
}

server::ServerConfig rootAuthSettings()
{
    server::ServerConfig config;
    config.setMode(AuthenticationMode::password_protected);
    config.setPassphrase("session-secret");
    config.setRootPassword("root-secret");
    return config;
}

TEST(AuthCommand, PassphraseCommandCannotGrantCommonAccess)
{
    server::Command command(rootAuthSettings());
    server::User user(1, "test-address");
    AuthTestProtocol protocol(NXProtocol::Type::BOOST_TCP_SERVER);

    EXPECT_EQ(command.read(authCommand(2, "session-secret"), user, protocol, 1),
              server::CommandResult::unauthorized);
    EXPECT_FALSE(user.hasCommonAccess());
}

TEST(AuthCommand, RootCommandRequiresAuthenticatedTlsTcpSession)
{
    auto config = rootAuthSettings();
    server::Command command(config);
    server::User user(1, "test-address");
    AuthTestProtocol tcp(NXProtocol::Type::BOOST_TCP_SERVER);
    AuthTestProtocol udp(NXProtocol::Type::BOOST_UDP_SERVER);
    const auto correct = authCommand(1, "root-secret");

    EXPECT_EQ(command.read(correct, user, tcp, 1), server::CommandResult::unauthorized);
    user.setCommonAccess(true);
    EXPECT_EQ(command.read(correct, user, udp, 2), server::CommandResult::unauthorized);

    config.setTls(false);
    server::Command plaintext(config);
    EXPECT_EQ(plaintext.read(correct, user, tcp, 3), server::CommandResult::unauthorized);

    config.setTls(true);
    config.setMode(AuthenticationMode::skip);
    server::Command skip(config);
    EXPECT_EQ(skip.read(correct, user, tcp, 4), server::CommandResult::unauthorized);
    EXPECT_FALSE(user.hasRootAccess());
}

TEST(AuthCommand, RootCommandAllowsAuthenticatedClient)
{
    server::Command command(rootAuthSettings());
    server::User user(1, "test-address");
    user.setCommonAccess(true);
    AuthTestProtocol tcp(NXProtocol::Type::BOOST_TCP_SERVER);

    EXPECT_EQ(command.read(authCommand(1, "wrong"), user, tcp, 1), server::CommandResult::unauthorized);
    EXPECT_EQ(command.read(authCommand(1, "root-secret"), user, tcp, 2), server::CommandResult::success);
    EXPECT_TRUE(user.hasRootAccess());
}

TEST(AuthCommand, RootCommandRejectsUnsetRootPassword)
{
    auto config = rootAuthSettings();
    config.setRootPassword("");
    server::Command command(config);
    server::User user(1, "test-address");
    user.setCommonAccess(true);
    AuthTestProtocol tcp(NXProtocol::Type::BOOST_TCP_SERVER);

    EXPECT_EQ(command.read(authCommand(1, ""), user, tcp, 1), server::CommandResult::unauthorized);
    EXPECT_FALSE(user.hasRootAccess());
}

TEST(AuthCommand, RootFailuresLockOutAddressAcrossSessions)
{
    server::Command command(rootAuthSettings());
    server::User user(1, "test-address");
    user.setCommonAccess(true);
    AuthTestProtocol tcp(NXProtocol::Type::BOOST_TCP_SERVER);

    for (uint64_t messageId = 1; messageId <= 5; ++messageId)
    {
        EXPECT_EQ(command.read(authCommand(1, "wrong"), user, tcp, messageId),
                  server::CommandResult::unauthorized);
    }
    EXPECT_EQ(command.read(authCommand(1, "root-secret"), user, tcp, 6),
              server::CommandResult::unauthorized);
    EXPECT_FALSE(user.hasRootAccess());

    server::User reconnected(2, "test-address");
    reconnected.setCommonAccess(true);
    EXPECT_EQ(command.read(authCommand(1, "root-secret"), reconnected, tcp, 7),
              server::CommandResult::unauthorized);
    EXPECT_FALSE(reconnected.hasRootAccess());

    server::User otherAddress(3, "other-address");
    otherAddress.setCommonAccess(true);
    EXPECT_EQ(command.read(authCommand(1, "root-secret"), otherAddress, tcp, 8),
              server::CommandResult::success);
    EXPECT_TRUE(otherAddress.hasRootAccess());
}

class MessageHandlerPasswordProtectedTest : public ::testing::Test
{
protected:
    server::ServerConfig config{AuthenticationMode::password_protected, "secret"};
    server::MessageHandler handler;

    void TearDown() override
    {
        server::ClientStorage::clear();
    }

    std::unique_ptr<server::BaseMessage> sendPassword(const std::string& password)
    {
        return handler.readMessage("test_address", Util::convertToByteVector(password), &config);
    }
};

TEST_F(MessageHandlerPasswordProtectedTest, CorrectShortPasswordAuthenticates)
{
    auto result = sendPassword("secret");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getType(), server::BaseMessage::Type::auth_message);
}

TEST_F(MessageHandlerPasswordProtectedTest, WrongShortPasswordReturnsAuthError)
{
    auto result = sendPassword("abc");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getType(), server::BaseMessage::Type::error_message);
}

TEST_F(MessageHandlerPasswordProtectedTest, WrongLongPasswordReturnsAuthError)
{
    auto result = sendPassword("a-much-longer-wrong-password");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getType(), server::BaseMessage::Type::error_message);
}

TEST_F(MessageHandlerPasswordProtectedTest, CorrectLengthWrongPasswordReturnsAuthError)
{
    auto result = sendPassword("secrer");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getType(), server::BaseMessage::Type::error_message);
}

TEST_F(MessageHandlerPasswordProtectedTest, EmptyPasswordReturnsAuthError)
{
    auto result = sendPassword("");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getType(), server::BaseMessage::Type::error_message);
}

TEST_F(MessageHandlerPasswordProtectedTest, RepeatedFailuresLockOutAddress)
{
    for (int i = 0; i < 5; ++i)
    {
        auto result = sendPassword("wrong");
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->getType(), server::BaseMessage::Type::error_message);
    }

    // The correct passphrase is rejected while the source address is locked out.
    auto blocked = sendPassword("secret");
    ASSERT_NE(blocked, nullptr);
    EXPECT_EQ(blocked->getType(), server::BaseMessage::Type::error_message);

    // A different source address is unaffected by the lockout.
    auto other = handler.readMessage("other_address", Util::convertToByteVector("secret"), &config);
    ASSERT_NE(other, nullptr);
    EXPECT_EQ(other->getType(), server::BaseMessage::Type::auth_message);
}

TEST_F(MessageHandlerPasswordProtectedTest, SuccessfulAuthClearsFailureState)
{
    for (int i = 0; i < 4; ++i)
    {
        auto result = sendPassword("wrong");
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->getType(), server::BaseMessage::Type::error_message);
    }

    auto result = sendPassword("secret");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getType(), server::BaseMessage::Type::auth_message);

    // Another user from the same address starts with a fresh counter.
    for (int i = 0; i < 4; ++i)
    {
        auto retry = handler.readMessage("test_address", Util::convertToByteVector("wrong"), &config);
        ASSERT_NE(retry, nullptr);
        EXPECT_EQ(retry->getType(), server::BaseMessage::Type::error_message);
    }
    auto stillAllowed = sendPassword("secret");
    ASSERT_NE(stillAllowed, nullptr);
    EXPECT_EQ(stillAllowed->getType(), server::BaseMessage::Type::auth_message);
}

class MessageHandlerSkipTest : public ::testing::Test
{
protected:
    server::ServerConfig config{AuthenticationMode::skip, ""};
    server::MessageHandler handler;

    void TearDown() override
    {
        server::ClientStorage::clear();
    }

    // Build a framed command payload exactly as the skip-mode client sends it:
    // 8 byte client id (0 for a brand new client) + 8 byte message id +
    // the "getting" / "client_id" command bytes.
    std::unique_ptr<server::BaseMessage> sendHandshake()
    {
        nx_data payload;
        auto clientIdBytes = Util::convertToByteVector(static_cast<uint64_t>(0));
        auto messageIdBytes = Util::convertToByteVector(static_cast<uint64_t>(100));
        payload.insert(payload.end(), clientIdBytes.begin(), clientIdBytes.end());
        payload.insert(payload.end(), messageIdBytes.begin(), messageIdBytes.end());
        payload.emplace_back(static_cast<uint8_t>(CommandType::getting));
        payload.emplace_back(0);
        payload.emplace_back(0);
        return handler.readMessage("test_address", payload, &config);
    }
};

TEST_F(MessageHandlerSkipTest, NewClientReceivesAuthMessageWithId)
{
    auto result = sendHandshake();
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getType(), server::BaseMessage::Type::auth_message);

    // The user must now be registered so its client id stays stable.
    auto stored = server::ClientStorage::getClientsByIpAddress("test_address");
    ASSERT_FALSE(stored.empty());
    EXPECT_NE(stored[0]->getId(), 0);
}

TEST_F(MessageHandlerSkipTest, SecondMessageFromSameClientIsHandledAsCommand)
{
    auto first = sendHandshake();
    ASSERT_NE(first, nullptr);
    ASSERT_EQ(first->getType(), server::BaseMessage::Type::auth_message);

    auto stored = server::ClientStorage::getClientsByIpAddress("test_address");
    ASSERT_FALSE(stored.empty());
    auto clientId = stored[0]->getId();

    // Send a "getting" / "client_id" command from the now-registered client
    // using its assigned id. This must be processed as a normal command.
    nx_data payload;
    auto clientIdBytes = Util::convertToByteVector(clientId);
    auto messageIdBytes = Util::convertToByteVector(static_cast<uint64_t>(200));
    payload.insert(payload.end(), clientIdBytes.begin(), clientIdBytes.end());
    payload.insert(payload.end(), messageIdBytes.begin(), messageIdBytes.end());
    payload.emplace_back(static_cast<uint8_t>(CommandType::getting));
    payload.emplace_back(0);
    payload.emplace_back(0);

    auto signedPayload = message_auth::sign(payload, stored[0]->getUdpToken());
    ASSERT_FALSE(signedPayload.empty());
    auto second = handler.readMessage("test_address", signedPayload, &config);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->getType(), server::BaseMessage::Type::message);

    // The user id must not have changed between messages.
    auto again = server::ClientStorage::getClientsByIpAddress("test_address");
    ASSERT_FALSE(again.empty());
    EXPECT_EQ(again[0]->getId(), clientId);
}

} // namespace
