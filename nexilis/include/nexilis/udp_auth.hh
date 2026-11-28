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

#ifndef NEXILIS_UDP_AUTH_HH
#define NEXILIS_UDP_AUTH_HH

#include <nexilis/crypto.hh>
#include <nexilis/message_auth.hh>
#include <nexilis/nx_data.hh>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace nexilis::udp_auth
{

inline constexpr std::array<uint8_t, 4> hello{'N', 'X', 'U', 'H'};
inline constexpr std::array<uint8_t, 4> challenge{'N', 'X', 'U', 'C'};
inline constexpr std::array<uint8_t, 4> authentication{'N', 'X', 'U', 'A'};
inline constexpr std::array<uint8_t, 4> session{'N', 'X', 'U', 'S'};
inline constexpr std::array<uint8_t, 4> response{'N', 'X', 'U', 'R'};
inline constexpr size_t token_size = 16;
inline constexpr size_t cookie_size = 16;

inline bool hasPrefix(const nx_data& packet, const std::array<uint8_t, 4>& prefix)
{
    return packet.size() >= prefix.size() &&
           std::equal(prefix.begin(), prefix.end(), packet.begin());
}

inline int hexValue(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

inline std::string toHex(const uint8_t* bytes, size_t count)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(count * 2);
    for (size_t i = 0; i < count; ++i)
    {
        result += digits[bytes[i] >> 4];
        result += digits[bytes[i] & 15];
    }
    return result;
}

inline bool fromHex(std::string_view hex, std::array<uint8_t, token_size>& bytes)
{
    if (hex.size() != token_size * 2)
        return false;
    for (size_t i = 0; i < token_size; ++i)
    {
        int high = hexValue(hex[i * 2]);
        int low = hexValue(hex[i * 2 + 1]);
        if (high < 0 || low < 0)
            return false;
        bytes[i] = static_cast<uint8_t>((high << 4) | low);
    }
    return true;
}

/// Derive a 32-byte AES-256-GCM key from the 16-byte per-session UDP token via
/// SHA-256. Client id and message id stay in plaintext so the server can route
/// the packet before decryption; only the command payload is encrypted.
inline std::array<uint8_t, crypto::kKeyLength> sessionKey(std::string_view tokenHex)
{
    std::array<uint8_t, token_size> token{};
    if (!fromHex(tokenHex, token))
        return {};
    unsigned char digest[EVP_MAX_MD_SIZE]{};
    unsigned int digestSize = 0;
    if (!EVP_Digest(token.data(), token.size(), digest, &digestSize, EVP_sha256(), nullptr) ||
        digestSize < crypto::kKeyLength)
        return {};
    std::array<uint8_t, crypto::kKeyLength> key{};
    std::copy_n(digest, key.size(), key.begin());
    return key;
}

inline nx_data makeHello()
{
    nx_data packet(hello.begin(), hello.end());
    packet.resize(hello.size() + cookie_size, 0);
    return packet;
}

inline nx_data makeAuthentication(const nx_data& cookie, const nx_data& payload)
{
    nx_data packet(authentication.begin(), authentication.end());
    packet.insert(packet.end(), cookie.begin(), cookie.end());
    packet.insert(packet.end(), payload.begin(), payload.end());
    return packet;
}

inline nx_data makeSession(const nx_data& payload, std::string_view token)
{
    if (payload.size() < message_auth::request_header_size + 2)
        return {};
    auto key = sessionKey(token);
    if (key.empty())
        return {};
    // Client id and message id stay in plaintext so the server can route the
    // packet before decryption. Only the command payload is encrypted.
    nx_data header(payload.begin(), payload.begin() + message_auth::request_header_size);
    nx_data command(payload.begin() + message_auth::request_header_size, payload.end());
    std::string sealed;
    if (!crypto::encrypt(std::string(key.begin(), key.end()),
                         std::string(command.begin(), command.end()),
                         sealed))
        return {};
    nx_data encryptedCommand(sealed.begin(), sealed.end());
    nx_data toSign(header);
    toSign.insert(toSign.end(), encryptedCommand.begin(), encryptedCommand.end());
    auto signedPayload = message_auth::sign(toSign, token);
    if (signedPayload.empty())
        return {};
    nx_data packet(session.begin(), session.end());
    packet.insert(packet.end(), signedPayload.begin(), signedPayload.end());
    return packet;
}

inline bool readSession(const nx_data& packet, std::string_view token, nx_data& payload)
{
    if (!hasPrefix(packet, session) || packet.size() < 4 + 8 + 8 + 2 + message_auth::tag_size)
        return false;
    nx_data signedPayload(packet.begin() + 4, packet.end());
    nx_data verified;
    if (!message_auth::verify(signedPayload, token, verified))
        return false;
    // verified = [client_id(8)][message_id(8)][encrypted_command]. Decrypt the
    // command payload and rebuild [client_id][message_id][plaintext_command].
    if (verified.size() < message_auth::request_header_size)
        return false;
    auto key = sessionKey(token);
    if (key.empty())
        return false;
    nx_data encryptedCommand(verified.begin() + message_auth::request_header_size,
                             verified.end());
    std::string sealed(encryptedCommand.begin(), encryptedCommand.end());
    std::string decryptedStr;
    if (!crypto::decrypt(std::string(key.begin(), key.end()), sealed, decryptedStr))
        return false;
    payload.assign(verified.begin(), verified.begin() + message_auth::request_header_size);
    payload.insert(payload.end(), decryptedStr.begin(), decryptedStr.end());
    return true;
}

inline std::optional<std::array<uint8_t, message_auth::tag_size>> responseTag(
        const nx_data& data, std::string_view token)
{
    std::array<uint8_t, token_size> key{};
    if (!message_auth::decodeKey(token, key))
        return std::nullopt;
    constexpr std::string_view domain = "Nexilis UDP response v1";
    nx_data input(domain.begin(), domain.end());
    input.insert(input.end(), data.begin(), data.end());
    unsigned int length = 0;
    unsigned char digest[EVP_MAX_MD_SIZE]{};
    if (!HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()), input.data(),
              input.size(), digest, &length) ||
        length < message_auth::tag_size)
        return std::nullopt;
    std::array<uint8_t, message_auth::tag_size> tag{};
    std::copy_n(digest, tag.size(), tag.begin());
    return tag;
}

inline nx_data makeResponse(const nx_data& payload, std::string_view token, uint64_t sequence)
{
    if (sequence == 0)
        return {};
    auto key = sessionKey(token);
    if (key.empty())
        return {};
    // Encrypt the response payload before wrapping it in the response envelope.
    std::string sealed;
    if (!crypto::encrypt(std::string(key.begin(), key.end()),
                         std::string(payload.begin(), payload.end()),
                         sealed))
        return {};
    nx_data encryptedPayload(sealed.begin(), sealed.end());
    nx_data data(response.begin(), response.end());
    for (int shift = 56; shift >= 0; shift -= 8)
        data.push_back(static_cast<uint8_t>(sequence >> shift));
    data.insert(data.end(), encryptedPayload.begin(), encryptedPayload.end());
    auto tag = responseTag(data, token);
    if (!tag)
        return {};
    data.insert(data.begin() + 12, tag->begin(), tag->end());
    return data;
}

inline bool readResponse(const nx_data& packet, std::string_view token,
                         uint64_t& sequence, nx_data& payload)
{
    if (!hasPrefix(packet, response) || packet.size() <= 12 + message_auth::tag_size)
        return false;
    nx_data signedData(packet.begin(), packet.begin() + 12);
    signedData.insert(signedData.end(), packet.begin() + 12 + message_auth::tag_size,
                      packet.end());
    auto tag = responseTag(signedData, token);
    if (!tag || CRYPTO_memcmp(tag->data(), packet.data() + 12, tag->size()) != 0)
        return false;
    sequence = 0;
    for (size_t i = 4; i < 12; ++i)
        sequence = (sequence << 8) | packet[i];
    if (sequence == 0)
        return false;
    // payload is encrypted; decrypt it after MAC verification.
    payload.assign(packet.begin() + 12 + message_auth::tag_size, packet.end());
    auto key = sessionKey(token);
    if (key.empty())
        return false;
    std::string sealed(payload.begin(), payload.end());
    std::string decryptedStr;
    if (!crypto::decrypt(std::string(key.begin(), key.end()), sealed, decryptedStr))
        return false;
    payload.assign(decryptedStr.begin(), decryptedStr.end());
    return true;
}

// A stateless, source-address-and-port-bound challenge. Only a peer that can
// receive the challenge can complete initial UDP authentication. The reply is
// exactly the same size as the required hello (20 bytes).
inline std::optional<std::array<uint8_t, cookie_size>> cookieFor(
        const std::array<uint8_t, 32>& key, std::string_view endpoint, uint64_t bucket)
{
    std::string input(endpoint);
    input += ':';
    input += std::to_string(bucket);
    unsigned int digest_size = 0;
    unsigned char digest[EVP_MAX_MD_SIZE]{};
    if (!HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
              reinterpret_cast<const unsigned char*>(input.data()), input.size(), digest, &digest_size) ||
        digest_size < cookie_size)
        return std::nullopt;
    std::array<uint8_t, cookie_size> result{};
    std::copy_n(digest, cookie_size, result.begin());
    return result;
}

inline uint64_t currentBucket()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                                         std::chrono::steady_clock::now().time_since_epoch())
                                         .count() /
                                 60);
}

inline nx_data makeChallenge(const std::array<uint8_t, 32>& key, std::string_view endpoint)
{
    auto cookie = cookieFor(key, endpoint, currentBucket());
    if (!cookie)
        return {};
    // Copy the cookie out of the optional and assemble the packet in a single
    // exactly sized buffer. Deriving the copy source through
    // std::optional::operator-> inside vector::insert makes GCC 13 emit a
    // false-positive -Warray-bounds warning when this is inlined into
    // UDPServer::receiveFromClients.
    const std::array<uint8_t, cookie_size> cookieBytes = *cookie;
    nx_data packet(challenge.size() + cookie_size, 0);
    std::copy(challenge.begin(), challenge.end(), packet.begin());
    std::copy(cookieBytes.begin(), cookieBytes.end(), packet.begin() + challenge.size());
    return packet;
}

inline bool checkAuthentication(const nx_data& packet, const std::array<uint8_t, 32>& key,
                                std::string_view endpoint, nx_data& payload)
{
    if (!hasPrefix(packet, authentication) || packet.size() <= 4 + cookie_size)
        return false;
    auto bucket = currentBucket();
    auto current = cookieFor(key, endpoint, bucket);
    auto previous = bucket > 0 ? cookieFor(key, endpoint, bucket - 1) : std::nullopt;
    if (!current)
        return false;
    const auto* received = packet.data() + 4;
    if (CRYPTO_memcmp(current->data(), received, cookie_size) != 0 &&
        (!previous || CRYPTO_memcmp(previous->data(), received, cookie_size) != 0))
        return false;
    payload.assign(packet.begin() + 4 + cookie_size, packet.end());
    return true;
}

} // namespace nexilis::udp_auth

#endif
