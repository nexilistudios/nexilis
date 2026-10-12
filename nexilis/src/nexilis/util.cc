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

#include <nexilis/logger/log.hh>
#include <nexilis/nexilis_constants.hh>
#include <nexilis/nx_util.hh>
#include <nexilis/util.hh>

#include <boost/json/serialize.hpp>

#include <bitset>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

namespace nexilis
{

static std::random_device rand_dev;

std::string Util::convertToString(nx_data bytes)
{
    std::string result;
    for (uint8_t b : bytes)
    {
        result += static_cast<char>(b);
    }
    return result;
}

bool Util::constantTimeEquals(const std::string& expected, const std::string& given)
{
    uint8_t result = static_cast<uint8_t>(expected.size() != given.size());

    for (size_t i = 0; i < expected.size(); ++i)
    {
        uint8_t expectedByte = static_cast<uint8_t>(expected[i]);
        uint8_t givenByte = i < given.size() ? static_cast<uint8_t>(given[i]) : 0;
        result |= static_cast<uint8_t>(expectedByte ^ givenByte);
    }

    return result == 0;
}

bool Util::constantTimeEquals(const std::string& expected, const nx_data& given)
{
    uint8_t result = static_cast<uint8_t>(expected.size() != given.size());

    for (size_t i = 0; i < expected.size(); ++i)
    {
        uint8_t expectedByte = static_cast<uint8_t>(expected[i]);
        uint8_t givenByte = i < given.size() ? static_cast<uint8_t>(given[i]) : 0;
        result |= static_cast<uint8_t>(expectedByte ^ givenByte);
    }

    return result == 0;
}

std::string Util::convertToNumbers(const nx_data& bytes)
{
    std::stringstream ss;
    for (uint8_t byte : bytes)
    {
        ss << std::hex << static_cast<int>(byte) << " ";
    }
    ss << std::dec << std::endl;
    return ss.str();
}

uint16_t Util::convertoToUint16(const nx_data& bytes)
{
    if (bytes.size() < 2)
    {
        throw std::invalid_argument("Input nx_data must contain at least 2 bytes");
    }

    if (server::Config::getBigEndian())
    {
        return static_cast<uint16_t>((bytes[0] << 8) | bytes[1]);
    }
    else
    {
        return static_cast<uint16_t>(bytes[0] | static_cast<uint16_t>(bytes[1]) << 8);
    }
}

// Big-endian float conversion helper.
float bytesToFloatBigEndian(const uint8_t* bytes)
{
    uint32_t temp = 0;
    temp |= static_cast<uint32_t>(bytes[0]) << 24;
    temp |= static_cast<uint32_t>(bytes[1]) << 16;
    temp |= static_cast<uint32_t>(bytes[2]) << 8;
    temp |= static_cast<uint32_t>(bytes[3]);

    float result;
    memcpy(&result, &temp, sizeof(float));
    return result;
}

Vector2f Util::convertToVector2(const nx_data& bytes)
{
    // Ensure the vector has enough bytes for two floats
    if (bytes.size() < sizeof(float) * 2)
    {
        throw std::invalid_argument("The input vector does not contain enough bytes for two floats.");
    }
    float x, y;

    if (server::Config::getBigEndian())
    {
        x = bytesToFloatBigEndian(bytes.data());
        y = bytesToFloatBigEndian(bytes.data() + sizeof(float));
    }
    else
    {
        // Copy the first 4 bytes.
        memcpy(&x, bytes.data(), sizeof(float));
        // Copy the next 4 bytes.
        memcpy(&y, bytes.data() + sizeof(float), sizeof(float));
    }
    return Vector2f(x, y);
}

Vector3f Util::convertToVector3(const nx_data& bytes)
{
    if (bytes.size() < sizeof(float) * 3)
    {
        throw std::invalid_argument("The input vector does not contain enough bytes for three floats");
    }
    float x, y, z;

    if (server::Config::getBigEndian())
    {
        x = bytesToFloatBigEndian(bytes.data());
        y = bytesToFloatBigEndian(bytes.data() + sizeof(float));
        z = bytesToFloatBigEndian(bytes.data() + 2 * sizeof(float));
    }
    else
    {
        memcpy(&x, bytes.data(), sizeof(float));
        memcpy(&y, bytes.data() + sizeof(float), sizeof(float));
        memcpy(&z, bytes.data() + sizeof(float) * 2, sizeof(float));
    }
    return Vector3f(x, y, z);
}

uint16_t Util::uint8PairToUint16(uint8_t lowByte, uint8_t highByte)
{
    return static_cast<uint16_t>(static_cast<uint16_t>(lowByte) | (static_cast<uint16_t>(highByte) << 8));
}

void Util::uint16ToUint8Pair(uint16_t value, uint8_t& lowByte, uint8_t& highByte)
{
    lowByte = static_cast<uint8_t>(value & 0xFF);
    highByte = static_cast<uint8_t>((value >> 8) & 0xFF);
}

nx_data Util::convertToByteVector(uint64_t value)
{
    nx_data result(sizeof(uint64_t));

    if (server::Config::getBigEndian())
    {
        for (uint64_t i = 0; i < sizeof(uint64_t); ++i)
        {
            result[sizeof(uint64_t) - 1 - i] = static_cast<uint8_t>((value >> (8 * i)) & 0xFF);
        }
    }
    else
    {
        for (uint64_t i = 0; i < sizeof(uint64_t); ++i)
        {
            result[i] = static_cast<uint8_t>((value >> (8 * i)) & 0xFF);
        }
    }
    return result;
}

nx_data Util::convertToByteVector(uint16_t value)
{
    nx_data result(sizeof(uint16_t));

    if (server::Config::getBigEndian())
    {
        for (uint16_t i = 0; i < sizeof(uint16_t); ++i)
        {
            result[sizeof(uint16_t) - 1 - i] = static_cast<uint8_t>((value >> (8 * i) & 0xFF));
        }
    }
    else
    {
        for (uint16_t i = 0; i < sizeof(uint16_t); ++i)
        {
            result[i] = static_cast<uint8_t>((value >> (8 * i) & 0xFF));
        }
    }
    return result;
}

nx_data Util::convertToByteVector(float value)
{
    nx_data bytes(sizeof(float));
    memcpy(bytes.data(), &value, sizeof(float));
    return bytes;
}

nx_data Util::convertToByteVector(Vector2f value)
{
    auto vec1 = convertToByteVector(value.x);
    auto vec2 = convertToByteVector(value.y);
    vec1.insert(vec1.end(), vec2.begin(), vec2.end());
    return vec1;
}

nx_data Util::convertToByteVector(Vector3f vector)
{
    auto x = convertToByteVector(vector.x);
    auto y = convertToByteVector(vector.y);
    auto z = convertToByteVector(vector.z);
    return nx_create(x, y, z);
}

nx_data Util::convertToByteVector(const char* command_data, uint64_t length)
{
    nx_data result;
    result.reserve(length);

    for (size_t i = 0; i < length; i++)
    {
        result.emplace_back(static_cast<uint8_t>(command_data[i]));
    }
    return result;
}

nx_data Util::convertToByteVector(const std::string& value)
{
    return convertToByteVector(value.c_str(), value.size());
}

nx_data Util::convertToByteVector(const boost::json::object& obj)
{
    std::string jsonString = boost::json::serialize(obj);
    nx_data byteStream(jsonString.begin(), jsonString.end());
    return byteStream;
}

nx_data Util::convertToByteVector(uint8_t value)
{
    return nx_data{value};
}

template <typename T>
T boostJsonValue(const std::function<bool()>& if_exists, const std::function<T()>& as)
{
    if (!if_exists())
    {
        throw std::runtime_error("Key does not exist or the value is off wrong type");
    }
    return as();
}

std::string Util::createString(const boost::json::value& ctx, const std::string& key)
{
    // clang-format off
    return boostJsonValue<std::string>(
            [&ctx, &key]() { return ctx.at(key).if_string(); },
            [&ctx, &key]() { return static_cast<std::string>(ctx.at(key).as_string()); });
    // clang-format on
}

uint64_t Util::createUint64(const boost::json::value& ctx, const std::string& key)
{
    const auto& val = ctx.at(key);
    if (auto* u = val.if_uint64())
    {
        return *u;
    }
    if (auto* i = val.if_int64())
    {
        return static_cast<uint64_t>(*i);
    }
    throw std::runtime_error("Key does not exist or the value is of wrong type");
}

double Util::createDouble(const boost::json::value& ctx, const std::string& key)
{
    // clang-format off
    return boostJsonValue<double>(
            [&ctx, &key]() { return ctx.at(key).if_double(); },
            [&ctx, &key]() { return static_cast<double>(ctx.at(key).as_double()); });
    // clang-format on
}

uint64_t Util::toUint64(const boost::json::value& val)
{
    if (val.is_uint64())
        return val.as_uint64();
    if (val.is_int64())
        return static_cast<uint64_t>(val.as_int64());
    return 0;
}

uint64_t Util::getRandomUint64()
{
    std::mt19937_64 generator(rand_dev());
    std::uniform_int_distribution<uint64_t> dist(0, NEXILIS_MAX);

    uint64_t randomValue;
    do
    {
        randomValue = dist(generator);
    } while (std::bitset<64>(randomValue).count() < 32); // Ensure at least 32 bits are set

    assert((std::is_same<decltype(randomValue), uint64_t>::value));

    if (randomValue == 0)
        return getRandomUint64();

    return randomValue;
}

int Util::getRandomInt(int from, int to)
{
    std::mt19937_64 generator(rand_dev());
    std::uniform_int_distribution<int> dist(from, to);
    int randomValue = dist(generator);

    assert(randomValue >= from);
    assert(randomValue <= to);
    return randomValue;
}

std::string Util::getRandomString(uint64_t charAmount)
{
    std::mt19937 eng(rand_dev());
    std::uniform_int_distribution<int> distribution('A', 'Z');

    std::string randomString;
    for (uint64_t i = 0; i < charAmount; ++i)
    {
        randomString += static_cast<char>(distribution(eng));
    }
    return randomString;
}

nx_data Util::removeAmountOfBytesFromVector(nx_data original, uint8_t amount)
{
    // Return empty vector if the original vector has less elements than we want to remove.
    if (original.size() < amount)
    {
        std::cerr << "Cannot remove more bytes than existing command has.";
        return {};
    }

    return nx_data(original.begin() + amount, original.end());
}

std::string Util::getColorMessage(logger::LogLevel logLevel, const std::string& data)
{
    std::string color;
    switch (logLevel)
    {
        case logger::LogLevel::Debug:
        case logger::LogLevel::Info:
            color = "\033[37m";
            break;
        case logger::LogLevel::Warning:
            color = "\033[33m";
            break;
        case logger::LogLevel::Error:
        case logger::LogLevel::Critical:
            color = "\033[31m";
            break;
    }
    std::stringstream ss;
    ss << color << data << "\033[0m" << std::endl;
    return ss.str();
}

void Util::printColorMessageToConsole(logger::LogLevel logLevel, const std::string& data)
{
    std::cout << getColorMessage(logLevel, data);
}

void Util::debugUint8Vector(const nx_data& vector)
{
    std::stringstream ss;
    for (uint8_t byte : vector)
    {
        ss << std::hex << static_cast<int>(byte) << " " << static_cast<char>(byte) << "\t";
    }
    ss << std::dec << std::endl;
    Log::debug(ss.str());
}

uint64_t Util::uint64FromFront(const nx_data& vec)
{
    if (vec.size() < 8)
    {
        // Not enough data to read 8 bytes
        Log::error("Not enough data to read 8 bytes");
        return 0;
    }

    uint64_t value = 0;

    if (server::Config::getBigEndian())
    {
        // Big-endian: Most significant byte is at the lowest address
        for (int i = 0; i < 8; ++i)
        {
            value |= static_cast<uint64_t>(vec[i]) << ((7 - i) * 8);
        }
    }
    else
    {
        // Little-endian: Least significant byte is at the lowest address
        for (int i = 0; i < 8; ++i)
        {
            value |= static_cast<uint64_t>(vec[i]) << (i * 8);
        }
    }
    return value;
}

float Util::floatFromFront(const nx_data& vec)
{
    if (vec.size() < 4)
    {
        Log::error("Not enough data to read 4 bytes");
        return 0.f;
    }

    float result;
    memcpy(&result, vec.data(), sizeof(float));
    return result;
}

Vector2f Util::vector2fFromFront(const nx_data& vector2)
{
    if (vector2.size() < 8)
    {
        Log::error("Not enough data to read Vector2f");
        return Vector2f();
    }

    Vector2f result;
    memcpy(&result.x, vector2.data(), sizeof(float));
    memcpy(&result.y, vector2.data() + sizeof(float), sizeof(float));
    return result;
}

Vector3f Util::vector3fFromFront(const nx_data& vector3)
{
    if (vector3.size() < 12)
    {
        Log::error("Not enough data to read Vector2f");
        return Vector3f();
    }

    Vector3f result;
    memcpy(&result.x, vector3.data(), sizeof(float));
    memcpy(&result.y, vector3.data() + sizeof(float), sizeof(float));
    memcpy(&result.z, vector3.data() + sizeof(float) * 2, sizeof(float));
    return result;
}

std::string Util::getDateAndTime()
{
    // Get the current time.
    auto now = std::chrono::system_clock::now();

    // Convert to time_t.
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);

    // Convert to local time struct.
    std::tm* localTime = std::localtime(&currentTime);

    // Format the time.
    std::stringstream ss;
    ss << std::put_time(localTime, "%Y-%m-%d_%H:%M:%S");
    return ss.str();
}

std::filesystem::path Util::getNexilisTempPath()
{
    auto tmp_dir = std::filesystem::temp_directory_path();
    auto nexilis_temp_dir = tmp_dir / "nexilis";

    std::error_code ec;
    std::filesystem::create_directories(nexilis_temp_dir, ec);
    if (!ec)
    {
        std::filesystem::permissions(nexilis_temp_dir,
                                     std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace,
                                     ec);
    }
    return nexilis_temp_dir;
}

void Util::deleteIfExists(const std::filesystem::path& filePath)
{
    try
    {
        if (std::filesystem::exists(filePath))
        {
            std::filesystem::remove(filePath);
        }
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        std::cerr << "Filesystem error: " << e.what() << '\n';
    }
}

std::string Util::getPortFilePath(NXProtocol::Type protocol_type)
{
    std::filesystem::path port_file = getNexilisTempPath() /
                                      (NXProtocol::typeToString(protocol_type) + "_port.txt");

    return port_file.string();
}

bool Util::writePortToFile(uint16_t port, NXProtocol::Type protocol_type)
{
    std::string file_path = getPortFilePath(protocol_type);

    try
    {
        std::ofstream port_file(file_path, std::ios::out);
        if (!port_file.is_open())
        {
            return false;
        }

        port_file << port;
        port_file.close();

        std::filesystem::permissions(file_path,
                                     std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                                     std::filesystem::perm_options::replace);

        return true;
    }
    catch (...)
    {
        return false;
    }
}

std::optional<uint16_t> Util::readPortFromFile(NXProtocol::Type protocol_type)
{
    std::string file_path = getPortFilePath(protocol_type);

    try
    {
        if (!std::filesystem::exists(file_path))
        {
            return std::nullopt;
        }

        std::ifstream port_file(file_path);
        if (!port_file.is_open())
        {
            return std::nullopt;
        }

        int port_value;
        if (!(port_file >> port_value))
        {
            return std::nullopt;
        }

        if (port_value < 1024 || port_value > 65535)
        {
            return std::nullopt;
        }

        return static_cast<uint16_t>(port_value);
    }
    catch (...)
    {
        return std::nullopt;
    }
}

void Util::cleanupPortFile(NXProtocol::Type protocol_type)
{
    std::string file_path = getPortFilePath(protocol_type);
    deleteIfExists(file_path);
}

} // namespace nexilis
