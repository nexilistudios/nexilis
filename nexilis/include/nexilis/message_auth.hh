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

#ifndef NEXILIS_MESSAGE_AUTH_HH
#define NEXILIS_MESSAGE_AUTH_HH

#include <nexilis/nx_data.hh>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace nexilis::message_auth
{

inline constexpr size_t key_size = 16;
inline constexpr size_t tag_size = 16;
inline constexpr size_t request_header_size = 8 + 8;
inline constexpr std::string_view domain = "Nexilis request v1";

inline bool decodeKey(std::string_view hex, std::array<uint8_t, key_size>& key)
{
    if (hex.size() != key_size * 2)
        return false;
    for (size_t i = 0; i < key_size; ++i)
    {
        auto digit = [](char c)
        {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
            return -1;
        };
        int high = digit(hex[i * 2]);
        int low = digit(hex[i * 2 + 1]);
        if (high < 0 || low < 0)
            return false;
        key[i] = static_cast<uint8_t>((high << 4) | low);
    }
    return true;
}

inline std::optional<std::array<uint8_t, tag_size>> tag(const nx_data& request,
                                                        std::string_view keyHex)
{
    std::array<uint8_t, key_size> key{};
    if (!decodeKey(keyHex, key))
        return std::nullopt;
    nx_data input(domain.begin(), domain.end());
    input.insert(input.end(), request.begin(), request.end());
    unsigned int digestSize = 0;
    unsigned char digest[EVP_MAX_MD_SIZE]{};
    if (!HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()), input.data(),
              input.size(), digest, &digestSize) ||
        digestSize < tag_size)
        return std::nullopt;
    std::array<uint8_t, tag_size> result{};
    std::copy_n(digest, tag_size, result.begin());
    return result;
}

inline nx_data sign(const nx_data& request, std::string_view keyHex)
{
    if (request.size() < request_header_size + 2)
        return {};
    auto mac = tag(request, keyHex);
    if (!mac)
        return {};
    nx_data signedRequest(request);
    signedRequest.insert(signedRequest.begin() + request_header_size, mac->begin(), mac->end());
    return signedRequest;
}

inline bool verify(const nx_data& signedRequest, std::string_view keyHex, nx_data& request)
{
    if (signedRequest.size() < request_header_size + tag_size + 2)
        return false;
    nx_data candidate(signedRequest.begin(), signedRequest.begin() + request_header_size);
    candidate.insert(candidate.end(), signedRequest.begin() + request_header_size + tag_size,
                     signedRequest.end());
    auto mac = tag(candidate, keyHex);
    if (!mac || CRYPTO_memcmp(mac->data(), signedRequest.data() + request_header_size, tag_size) != 0)
        return false;
    request = std::move(candidate);
    return true;
}

} // namespace nexilis::message_auth

#endif
