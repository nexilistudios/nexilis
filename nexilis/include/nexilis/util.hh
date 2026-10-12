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

#ifndef NEXILIS_COMMON_UTIL_HH
#define NEXILIS_COMMON_UTIL_HH

#include <nexilis/logger/log_level.hh>
#include <nexilis/nexilis_constants.hh>
#include <nexilis/nx_data.hh>
#include <nexilis/nx_protocol.hh>
#include <nexilis/server/config.hh>
#include <nexilis/types/vector2.hh>
#include <nexilis/types/vector3.hh>

#include <boost/json/object.hpp>

#include <filesystem>
#include <optional>
#include <string>

namespace nexilis
{

class Util
{
public:
    /// Convert nx_data to numeral type.
    template <typename T>
    static T convertToType(nx_data bytes)
    {
        if (bytes.size() < sizeof(T))
        {
            // Handle error: insufficient data
            throw std::runtime_error("Conversion failed: Insufficient data");
        }

        T result = 0;

        if (server::Config::getBigEndian())
        {
            for (uint64_t i = 0; i < sizeof(T); ++i)
            {
                result |= static_cast<T>(bytes[i]) << (8 * (sizeof(T) - 1 - i));
            }
        }
        else
        {
            for (uint64_t i = 0; i < sizeof(T); ++i)
            {
                result |= static_cast<T>(bytes[i]) << (8 * i);
            }
        }

        return result;
    }

    /// Convert nx_data to string.
    static std::string convertToString(nx_data bytes);

    /// Compare two strings in constant time.
    /// \param expected The secret value to compare against.
    /// \param given The attacker-controlled value to check.
    /// The execution time depends only on the length of the expected value, not on
    /// the length or contents of the given value. A length mismatch is folded into
    /// the result without branching, so a wrong-length password cannot reveal the
    /// length of the correct one.
    static bool constantTimeEquals(const std::string& expected, const std::string& given);

    /// Compare a secret string against raw bytes in constant time.
    /// \param expected The secret value to compare against.
    /// \param given The attacker-controlled byte payload to check.
    /// Same guarantees as the string overload, without converting the payload to
    /// a string first (the conversion itself would scale with the input length).
    static bool constantTimeEquals(const std::string& expected, const nx_data& given);

    /// Convert nx_data to number values.
    static std::string convertToNumbers(const nx_data& bytes);

    /// Convert nx_data to uint16_t
    static uint16_t convertoToUint16(const nx_data& bytes);

    /// Convert nx_data to Vec2f.
    static Vector2f convertToVector2(const nx_data& bytes);

    /// Convert nx_data to Vector3f
    static Vector3f convertToVector3(const nx_data& bytes);

    /// \defgroup BytevectorConversions Functions that converts items to byte vectors.

    /// \ingroup BytevectorConversions
    template <typename EnumType>
    static nx_data convertToByteVector(EnumType e,
                                       typename std::enable_if<std::is_enum<EnumType>::value>::type* = nullptr)
    {
        using UnderlyingType = typename std::underlying_type<EnumType>::type;
        return convertToByteVector(static_cast<uint8_t>(static_cast<UnderlyingType>(e)));
    }

    /// \ingroup BytevectorConversions
    static nx_data convertToByteVector(const char* command_data, uint64_t length);

    /// \ingroup BytevectorConversions
    static nx_data convertToByteVector(uint64_t value);

    /// \ingroup BytevectorConversions
    static nx_data convertToByteVector(uint16_t value);

    /// \ingroup BytevectorConversions
    static nx_data convertToByteVector(const boost::json::object& obj);

    /// \ingroup BytevectorConversions
    static nx_data convertToByteVector(float value);

    /// \ingroup BytevectorConversions
    static nx_data convertToByteVector(Vector2f value);

    /// \ingroup BytevectorConversions
    static nx_data convertToByteVector(Vector3f value);

    /// \ingroup BytevectorConversions
    static nx_data convertToByteVector(const std::string& value);

    /// \ingroup BytevectorConversions
    static nx_data convertToByteVector(uint8_t value);

    /// Get a boost json key value as a string.
    static std::string createString(const boost::json::value& ctx, const std::string& key);

    /// Get a boost json key value as a uint64.
    static uint64_t createUint64(const boost::json::value& ctx, const std::string& key);

    /// Get a boost json key value as a float.
    static double createDouble(const boost::json::value& ctx, const std::string& key);

    // Convert boost::json::value into uint64_t.
    static uint64_t toUint64(const boost::json::value& val);

    /// Return uint16_t from two bytes.
    static uint16_t uint8PairToUint16(uint8_t lowByte, uint8_t highByte);

    /// Separate two bytes.
    /// \param value The two bytes that are separate.
    /// \param lowByte The created low byte.
    /// \param hightByte The created high byte.
    static void uint16ToUint8Pair(uint16_t value, uint8_t& lowByte, uint8_t& highByte);

    /// \defgroup FrontConversions Functions that convert the first bytes of nx_data into type.

    /// Get the first eight bytes of vector and return it as uint64_t.
    /// \ingroup FrontConversions
    static uint64_t uint64FromFront(const nx_data& vec);

    /// Get the first four bytes from vector and return it as float.
    /// \ingroup FrontConversions
    static float floatFromFront(const nx_data& vec);

    /// Get the first eight bytes from vector and return it as Vector2f.
    /// \ingroup FrontConversions
    static Vector2f vector2fFromFront(const nx_data& vec);

    /// Get the first 12 bytes from vector and return it as Vector3f.
    /// \ingroup FrontConversions
    static Vector3f vector3fFromFront(const nx_data& vec);

    /// Remove amount of bytes from the beginning of the vector.
    /// \return The updated vector.
    static nx_data removeAmountOfBytesFromVector(nx_data original, uint8_t amount);

    /// \defgroup RandFunctions Functions that generate random values.

    /// Get random uint64_t value between 0 and max uint64.
    /// \ingroup RandFunctions
    static uint64_t getRandomUint64();

    /// Get random integer between values.
    /// \param from The smallest possible value.
    /// \param to The biggest possible value.
    /// \return The random integer.
    /// \ingroup RandFunctions
    static int getRandomInt(int from, int to);

    /// Get random characters from 'A' to 'Z'.
    /// \param charAmount The amount of characters in the string.
    /// \ingroup RandFunctions
    static std::string getRandomString(uint64_t charAmount);

    static std::string getDateAndTime();

    /// Logging.
    static std::string getColorMessage(logger::LogLevel logLevel, const std::string& data);
    static void printColorMessageToConsole(logger::LogLevel logLevel, const std::string& data);
    static void debugUint8Vector(const nx_data& vector);

    /// File stuff
    static void deleteIfExists(const std::filesystem::path& filePath);
    static std::filesystem::path getNexilisTempPath();

    /// \defgroup PortHandling

    /// \ingroup PortHandling
    static std::string getPortFilePath(NXProtocol::Type protocol_type);

    /// \ingroup PortHandling
    static bool writePortToFile(uint16_t port, NXProtocol::Type protocol_type);

    /// \ingroup PortHandling
    static std::optional<uint16_t> readPortFromFile(NXProtocol::Type protocol_type);

    /// \ingroup PortHandling
    static void cleanupPortFile(NXProtocol::Type protocol_type);
};

} // namespace nexilis

#endif
