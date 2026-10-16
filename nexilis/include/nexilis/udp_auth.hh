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

#include <nexilis/nx_data.hh>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

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
    std::array<uint8_t, token_size> bytes{};
    if (!fromHex(token, bytes) || payload.size() < 8)
        return {};
    nx_data packet(session.begin(), session.end());
    packet.insert(packet.end(), payload.begin(), payload.begin() + 8);
    packet.insert(packet.end(), bytes.begin(), bytes.end());
    packet.insert(packet.end(), payload.begin() + 8, payload.end());
    return packet;
}

inline bool readSession(const nx_data& packet, std::string_view token, nx_data& payload)
{
    std::array<uint8_t, token_size> expected{};
    if (!hasPrefix(packet, session) || packet.size() < 4 + 8 + token_size + 8 + 2 ||
        !fromHex(token, expected) ||
        CRYPTO_memcmp(expected.data(), packet.data() + 4 + 8, token_size) != 0)
        return false;
    payload.assign(packet.begin() + 4, packet.begin() + 4 + 8);
    payload.insert(payload.end(), packet.begin() + 4 + 8 + token_size, packet.end());
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
    nx_data packet(challenge.begin(), challenge.end());
    packet.insert(packet.end(), cookie->begin(), cookie->end());
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
