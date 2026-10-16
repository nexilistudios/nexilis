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

#include "../test_ports.hh"
#include <nexilis/client/client_api.hh>
#include <nexilis/client/packet.hh>
#include <nexilis/client/protocol/nxboost/tcp_client.hh>
#include <nexilis/logger/log.hh>
#include <nexilis/protocol_manager.hh>
#include <nexilis/server/protocol/nxboost/tcp_server.hh>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace
{

template <typename Predicate>
bool waitFor(Predicate&& predicate,
             std::chrono::milliseconds timeout = std::chrono::seconds(10))
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!predicate() && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return predicate();
}

/// Print all room messages that have not been shown yet.
void printNewMessages(nexilis::client::ClientAPI& api, uint64_t room_id,
                      const std::string& listener, std::size_t& printed)
{
    for (const auto& room : api.getActiveRooms())
    {
        if (room.getId() != room_id)
        {
            continue;
        }

        const auto& messages = room.getMessages();
        const auto count = messages.size();
        for (; printed < count; ++printed)
        {
            const auto& message = messages[printed];
            const auto* sender = message.getClient();
            const auto& name = sender ? sender->getUsername() : std::string("unknown");
            std::cout << "[" << listener << " heard " << name << "] " << message.getPayload() << std::endl;
        }
        return;
    }
}

/// Check whether a client's room snapshot already contains every expected message.
bool hasAllMessages(nexilis::client::ClientAPI& api, uint64_t room_id, std::size_t expected)
{
    for (const auto& room : api.getActiveRooms())
    {
        if (room.getId() == room_id)
        {
            return room.getMessages().size() >= expected;
        }
    }
    return false;
}

} // namespace

int main(int argc, char** argv)
{
    useTestPortsIfRequested(argc, argv);
    nexilis::Log::startConsoleDebugging();

    nexilis::ProtocolManager protocol_manager;

    // Server.
    nexilis::server::ServerConfig settings;
    settings.setMode(nexilis::server::AuthenticationMode::password_protected);
    settings.setPassphrase("salasana");
    settings.setRootPassword("root");

    auto server = protocol_manager.createProtocol<nexilis::server::nxboost::TCPServer>(settings);
    server.start();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Alice connects and hosts a chat room.
    nexilis::client::ClientConfig server_data;
    server_data.setPassword("salasana");
    server_data.setBoostTCPAddress("127.0.0.1");
    server_data.setMode(nexilis::server::AuthenticationMode::password_protected);

    nexilis::client::ClientAPI alice_api(server_data);
    auto alice = protocol_manager.createProtocol<nexilis::client::nxboost::TCPClient>(alice_api);
    alice.start();

    alice.sendMessage(nexilis::client::Packet::Set::General::username(alice_api, "Alice"));
    alice.sendMessage(nexilis::client::Packet::Room::Management::create(alice_api, "chat"));

    if (!waitFor([&]()
                 { return !alice_api.getActiveRooms().empty(); }))
    {
        std::cerr << "Alice failed to create the chat room" << std::endl;
        return EXIT_FAILURE;
    }

    const uint64_t room_id = alice_api.getActiveRooms()[0].getId();
    alice.sendMessage(nexilis::client::Packet::Room::Management::join(alice_api, room_id));

    if (!waitFor([&]()
                 { return alice_api.clientInRoom(); }))
    {
        std::cerr << "Alice failed to join the chat room" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Alice is hosting room \"chat\" (" << room_id << ")" << std::endl;

    // Bob connects, finds the chat room and joins it.
    nexilis::client::ClientConfig bob_data;
    bob_data.setPassword("salasana");
    bob_data.setBoostTCPAddress("127.0.0.1");
    bob_data.setMode(nexilis::server::AuthenticationMode::password_protected);

    nexilis::client::ClientAPI bob_api(bob_data);
    auto bob = protocol_manager.createProtocol<nexilis::client::nxboost::TCPClient>(bob_api);
    bob.start();

    bob.sendMessage(nexilis::client::Packet::Set::General::username(bob_api, "Bob"));
    bob.sendMessage(nexilis::client::Packet::Get::Info::rooms(bob_api));

    if (!waitFor([&]()
                 { return !bob_api.getActiveRooms().empty(); }))
    {
        std::cerr << "Bob failed to discover any rooms" << std::endl;
        return EXIT_FAILURE;
    }

    bob.sendMessage(nexilis::client::Packet::Room::Management::join(bob_api, room_id));

    if (!waitFor([&]()
                 { return bob_api.clientInRoom(); }))
    {
        std::cerr << "Bob failed to join the chat room" << std::endl;
        return EXIT_FAILURE;
    }

    if (!waitFor([&]()
                 {
                 for (const auto& room : alice_api.getActiveRooms())
                 {
                     if (room.getId() == room_id && room.getClients().size() == 2)
                     {
                         return true;
                     }
                 }
                 return false; }))
    {
        std::cerr << "Alice never saw Bob join the chat room" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Bob joined the room, both clients are ready" << std::endl;

    // The conversation. Every broadcast is delivered to everyone in the
    // room, including the sender.
    struct Turn
    {
        nexilis::client::nxboost::TCPClient& sender;
        nexilis::client::ClientAPI& api;
        std::string text;
    };

    const std::vector<Turn> conversation = {
            {alice, alice_api, "Hi Bob! Alice here."},
            {bob, bob_api, "Hey Alice! Nice to meet you."},
            {alice, alice_api, "How do you like Nexilis so far?"},
            {bob, bob_api, "It is surprisingly easy to use!"},
            {alice, alice_api, "Agreed. See you around, Bob!"},
            {bob, bob_api, "See you, Alice! Bye!"}};

    for (const auto& turn : conversation)
    {
        turn.sender.sendMessage(
                nexilis::client::Packet::Room::Communicate::broadcast(turn.api, turn.text));
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    // Re-run the same conversation with end-to-end encryption enabled. The
    // feature is optional: encryption only kicks in when the developer opts in.
    // The server relays the ciphertext opaquely, so it can no longer read the
    // messages. Every recipient decrypts them with the connection password.
    alice_api.setMessageEncryption(true);
    bob_api.setMessageEncryption(true);

    const std::vector<Turn> secret_conversation = {
            {alice, alice_api, "Pssst, Bob: the server can't read this one."},
            {bob, bob_api, "Impressive. Genuinely end-to-end encrypted."},
            {alice, alice_api, "And it only turned on because we asked for it."}};

    for (const auto& turn : secret_conversation)
    {
        turn.sender.sendMessage(
                nexilis::client::Packet::Room::Communicate::broadcast(turn.api, turn.text));
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    // Wait until both clients have received and displayed every message.
    const auto expected = conversation.size() + secret_conversation.size();
    std::size_t alice_printed = 0;
    std::size_t bob_printed = 0;

    const bool delivered = waitFor([&]()
                                   {
                                       printNewMessages(alice_api, room_id, "Alice", alice_printed);
                                       printNewMessages(bob_api, room_id, "Bob", bob_printed);
                                       return hasAllMessages(alice_api, room_id, expected) &&
                                              hasAllMessages(bob_api, room_id, expected); },
                                   std::chrono::seconds(15));

    // The encrypted messages must arrive as readable plaintext on both sides,
    // meaning the ciphertext actually round-tripped through the server.
    auto encryptedMessagesReadable = [&](nexilis::client::ClientAPI& api, const std::string& listener)
    {
        for (const auto& room : api.getActiveRooms())
        {
            if (room.getId() != room_id)
            {
                continue;
            }
            const auto& messages = room.getMessages();
            for (std::size_t index = conversation.size(); index < messages.size(); ++index)
            {
                const std::size_t turn = index - conversation.size();
                if (messages[index].getPayload() != secret_conversation[turn].text)
                {
                    std::cerr << listener << " received an unreadable encrypted message" << std::endl;
                    return false;
                }
            }
            return true;
        }
        return false;
    };

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    printNewMessages(alice_api, room_id, "Alice", alice_printed);
    printNewMessages(bob_api, room_id, "Bob", bob_printed);

    const bool encryptedOk = encryptedMessagesReadable(alice_api, "Alice") &&
                             encryptedMessagesReadable(bob_api, "Bob");
    if (encryptedOk)
    {
        std::cout << "End-to-end encrypted messages decrypted correctly on both clients" << std::endl;
    }

    bob.stop();
    std::cout << "Bob disconnected" << std::endl;

    alice.stop();
    std::cout << "Alice disconnected" << std::endl;

    server.stop();
    std::cout << "Server stopped" << std::endl;

    return delivered && encryptedOk ? EXIT_SUCCESS : EXIT_FAILURE;
}
