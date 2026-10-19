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

#include <nexilis/server/message/auth_message.hh>
#include <nexilis/server/message/error_message.hh>
#include <nexilis/server/message/message_handler.hh>

#include <nexilis/server/client_storage.hh>
#include <nexilis/server/command/command.hh>
#include <nexilis/server/config.hh>
#include <nexilis/server/room_storage.hh>
#include <nexilis/server/server_config.hh>
#include <nexilis/server/server_json.hh>

#include <nexilis/logger/log.hh>
#include <nexilis/message_auth.hh>

#include <nexilis/util.hh>

#include <algorithm>
#include <chrono>

namespace nexilis::server
{

namespace
{
constexpr unsigned kMaxPassphraseAuthFailures = 5;
constexpr auto kPassphraseAuthWindow = std::chrono::minutes(10);
constexpr size_t kMaxTrackedPassphraseAuthAddresses = 4096;
} // namespace

MessageHandler::MessageHandler()
    : NxClass("server::MessageHandler")
{
}

// Payload handled by this class:
// Client id 8 bytes
// Message id 8 bytes
// Command bytes (at least 2 bytes), second parameter of MessageHandler::Message.

std::unique_ptr<BaseMessage> MessageHandler::readMessage(std::string address, const nx_data& payload, ServerConfig* authentication)
{
    Log::debug(header(), "Payload size: ", payload.size());

    // In password_protected mode the very first message from an unidentified
    // client is the raw password, which may be shorter than 8 bytes. Only attempt
    // to read a client id when the payload is large enough; a short payload would
    // otherwise parse as a zero client id and get rejected before the password
    // comparison runs, leaking the length of the correct password.
    uint64_t clientId = 0;
    if (payload.size() >= sizeof(uint64_t))
    {
        clientId = Util::uint64FromFront(payload);
    }

    // Reject a zero client id in every mode except password_protected and skip:
    // in password_protected the first message from an unidentified client is the
    // password itself, and in skip mode the client has not been assigned an id
    // yet and sends a "getting client_id" handshake to learn it.
    if (clientId == 0 && authentication->getMode() != AuthenticationMode::password_protected && authentication->getMode() != AuthenticationMode::skip)
    {
        Log::error(header(), "Client id is zero");
        return std::make_unique<ErrorMessage>(address, ErrorMessage::Type::client_id_failure);
    }

    bool newUser = false;

    // Does the user actually exist?
    auto* user = ClientStorage::getClientById(clientId);
    std::unique_ptr<User> newUserPtr;
    if (!user)
    {
        // Create a new user.
        uint64_t newId = Util::getRandomUint64();
        newUserPtr = std::make_unique<User>(newId, address);
        Log::info(header(), "Created new user: ", newId);
        auto username = Util::getRandomString(10);
        newUserPtr->setUsername(username);
        user = newUserPtr.get();
        newUser = true;
    }
    assert(user);

    auto authenticatedMessage = [&]() -> std::unique_ptr<BaseMessage>
    {
        nx_data verified;
        if (!message_auth::verify(payload, user->getUdpToken(), verified))
        {
            Log::warning(header(), "Rejected request with an invalid session MAC");
            return nullptr;
        }
        nx_data idBytes(verified.begin() + 8, verified.begin() + 16);
        if (!user->acceptMessageId(Util::uint64FromFront(idBytes)))
        {
            Log::warning(header(), "Rejected duplicate or stale message ID");
            return nullptr;
        }
        return std::make_unique<Message>(handlePayload(verified, user, address));
    };

    switch (authentication->getMode())
    {
        case AuthenticationMode::empty:
        {
            return std::make_unique<ErrorMessage>(address, ErrorMessage::Type::empty_authentication_mode);
        }
        case AuthenticationMode::skip:
        {
            if (!newUser)
            {
                // Registered client: process the payload as a normal command.
                return authenticatedMessage();
            }

            // Brand new client. Register it so its client id stays stable and
            // hand its id back through the same AuthMessage handshake used by
            // password_protected mode (no credentials are verified here).
            if (newUserPtr)
            {
                ClientStorage::add(std::move(newUserPtr));
            }

            uint64_t newClientId = user->getId();
            auto realNewClient = ClientStorage::getClientById(newClientId);

            // Checking successfull client creation.
            assert(realNewClient);
            assert(user->getId() == realNewClient->getId());

            // This message is equal to Packet::getId (without client id):
            // a "getting" / "client_id" command that delivers the assigned id.
            // The session port is negotiated separately by the transport
            // (e.g. Boost TCP opens a switched session port after auth).
            nx_data message{1, 0, 0};
            auto idBytes = Util::convertToByteVector(user->getId());
            for (auto&& byte : idBytes)
            {
                message.emplace_back(byte);
            }
            std::vector<nx_data> test{message};

            auto new_message_id = Util::getRandomUint64();
            BaseMessage::Data base_data(new_message_id, address, realNewClient);
            return std::make_unique<AuthMessage>(std::move(base_data), test);
        }
        case AuthenticationMode::admin_access:
        case AuthenticationMode::root_access:
        {
            Log::error(header(), "Not implemented!");
            return std::make_unique<ErrorMessage>(address, ErrorMessage::Type::not_implemented);
        }
        case AuthenticationMode::password_protected:
        {
            if (!newUser)
            {
                if (user->hasCommonAccess())
                {
                    Log::debug(header(), "Message from known user");
                    return authenticatedMessage();
                }
                // Message from verified client that has no access.
                else
                {
                    Log::error(header(), "Message from verified client that has no access");
                    return std::make_unique<ErrorMessage>(address, ErrorMessage::Type::no_access);
                }
            }
            else
            {
                // Only accept the password as a message from unidentified clients.
                // Failed attempts are shared by source address across reconnects to
                // this server instance so the shared passphrase cannot be
                // brute-forced at line speed.
                bool blocked = false;
                {
                    const auto now = std::chrono::steady_clock::now();
                    std::lock_guard lock(m_passphraseAuthState->mutex);
                    auto& attempts = m_passphraseAuthState->byAddress;
                    auto it = attempts.find(address);
                    if (it != attempts.end() && now - it->second.firstFailure >= kPassphraseAuthWindow)
                    {
                        attempts.erase(it);
                        it = attempts.end();
                    }
                    if (it != attempts.end() && it->second.failures >= kMaxPassphraseAuthFailures)
                    {
                        blocked = true;
                    }
                    else if (it == attempts.end() &&
                             attempts.size() >= kMaxTrackedPassphraseAuthAddresses)
                    {
                        std::erase_if(attempts,
                                      [now](const auto& entry)
                                      {
                                          return now - entry.second.firstFailure >= kPassphraseAuthWindow;
                                      });
                        if (attempts.size() >= kMaxTrackedPassphraseAuthAddresses)
                        {
                            blocked = true;
                        }
                    }
                }

                if (blocked)
                {
                    Log::warning(header(), "Too many failed passphrase attempts from ", address);
                    return std::make_unique<ErrorMessage>(address, ErrorMessage::Type::authentication_error);
                }

                // The comparison runs over the stored password length only, so the
                // response does not reveal the length of the correct password.
                if (Util::constantTimeEquals(authentication->getPassphrase(), payload))
                {
                    Log::info(header(), "Correct password by user ", user->getId());

                    {
                        std::lock_guard lock(m_passphraseAuthState->mutex);
                        m_passphraseAuthState->byAddress.erase(address);
                    }

                    user->setCommonAccess(true);

                    // Add to storage only if this was a new user.
                    if (newUserPtr)
                    {
                        ClientStorage::add(std::move(newUserPtr));
                    }

                    uint64_t newClientId = user->getId();
                    auto realNewClient = ClientStorage::getClientById(newClientId);

                    // Checking successfull client creation.
                    assert(realNewClient);
                    assert(user->getId() == realNewClient->getId());

                    // This message is equal to Packet::getId (without client id).
                    // The session port is negotiated separately by the
                    // transport (e.g. Boost TCP opens a switched session
                    // port after auth completes).
                    nx_data message{1, 0, 0};
                    auto idBytes = Util::convertToByteVector(user->getId());
                    for (auto&& byte : idBytes)
                    {
                        message.emplace_back(byte);
                    }
                    std::vector<nx_data> test{message};

                    auto new_message_id = Util::getRandomUint64();
                    BaseMessage::Data base_data(new_message_id, address, realNewClient);
                    return std::make_unique<AuthMessage>(std::move(base_data), test);
                }
                else
                {
                    const auto now = std::chrono::steady_clock::now();
                    {
                        std::lock_guard lock(m_passphraseAuthState->mutex);
                        auto& attempts = m_passphraseAuthState->byAddress;
                        auto it = attempts.find(address);
                        if (it != attempts.end())
                        {
                            ++it->second.failures;
                        }
                        else
                        {
                            attempts.emplace(address, PassphraseAuthState::Attempt{now, 1});
                        }
                    }
                    Log::error(header(), "Authentication error: incorrect password");
                    return std::make_unique<ErrorMessage>(address, ErrorMessage::Type::authentication_error);
                }
            }
        }
        default:
        {
            Log::error(header(), "Missing authentication mode");
            return std::make_unique<ErrorMessage>(address, ErrorMessage::Type::missing_authentication_mode);
        }
    }
}

Message MessageHandler::handlePayload(const nx_data& payload, User* user, const std::string& address)
{
    // Vector without client id (8 bytes).
    auto vectorWithoutClientId = Util::removeAmountOfBytesFromVector(payload, 8);

    // Next eight bytes is the message id.
    uint64_t messageId = Util::uint64FromFront(vectorWithoutClientId);

    // Vector without message id (8 bytes).
    auto messageVector = Util::removeAmountOfBytesFromVector(vectorWithoutClientId, 8);

    BaseMessage::Data base_data(messageId, address, user);
    return Message(std::move(base_data), messageVector);
}

} // namespace nexilis::server
