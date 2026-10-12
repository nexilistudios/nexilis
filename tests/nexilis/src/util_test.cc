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

#include <nexilis/util.hh>

#include <boost/json/parse.hpp>
#include <cstring>
#include <fstream>

using namespace nexilis;

// ==================== convertToType Tests ====================

TEST(UtilConvertToTypeTest, Uint8)
{
    nx_data bytes = {0x2A};
    uint8_t result = Util::convertToType<uint8_t>(bytes);
    EXPECT_EQ(result, 0x2A);
}

TEST(UtilConvertToTypeTest, Uint16)
{
    nx_data bytes = {0x34, 0x12};
    uint16_t result = Util::convertToType<uint16_t>(bytes);
    if (server::Config::getBigEndian())
    {
        EXPECT_EQ(result, 0x3412);
    }
    else
    {
        EXPECT_EQ(result, 0x1234);
    }
}

TEST(UtilConvertToTypeTest, Uint64)
{
    nx_data bytes = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint64_t result = Util::convertToType<uint64_t>(bytes);
    if (server::Config::getBigEndian())
    {
        EXPECT_EQ(result, 0x0100000000000000ULL);
    }
    else
    {
        EXPECT_EQ(result, 0x01ULL);
    }
}

TEST(UtilConvertToTypeTest, InsufficientDataThrows)
{
    nx_data bytes = {0x01};
    EXPECT_THROW(Util::convertToType<uint64_t>(bytes), std::runtime_error);
}

// ==================== convertToString Tests ====================

TEST(UtilConvertToStringTest, ConvertsBytes)
{
    nx_data bytes = {'H', 'e', 'l', 'l', 'o'};
    EXPECT_EQ(Util::convertToString(bytes), "Hello");
}

TEST(UtilConvertToStringTest, EmptyBytes)
{
    nx_data bytes;
    EXPECT_EQ(Util::convertToString(bytes), "");
}

// ==================== constantTimeEquals Tests ====================

TEST(UtilConstantTimeTest, EqualStrings)
{
    EXPECT_TRUE(Util::constantTimeEquals("hello", "hello"));
}

TEST(UtilConstantTimeTest, DifferentStrings)
{
    EXPECT_FALSE(Util::constantTimeEquals("hello", "world"));
}

TEST(UtilConstantTimeTest, DifferentLengths)
{
    EXPECT_FALSE(Util::constantTimeEquals("hello", "hi"));
}

TEST(UtilConstantTimeTest, EmptyStrings)
{
    EXPECT_TRUE(Util::constantTimeEquals("", ""));
}

TEST(UtilConstantTimeTest, NxDataMatch)
{
    nx_data data = {'h', 'e', 'l', 'l', 'o'};
    EXPECT_TRUE(Util::constantTimeEquals("hello", data));
}

TEST(UtilConstantTimeTest, NxDataMismatch)
{
    nx_data data = {'h', 'e', 'l', 'p'};
    EXPECT_FALSE(Util::constantTimeEquals("hello", data));
}

TEST(UtilConstantTimeTest, NxDataDifferentLength)
{
    nx_data data = {'h', 'i'};
    EXPECT_FALSE(Util::constantTimeEquals("hello", data));
}

// ==================== convertToNumbers Tests ====================

TEST(UtilConvertToNumbersTest, ConvertsHex)
{
    nx_data bytes = {0x0A, 0xFF};
    std::string result = Util::convertToNumbers(bytes);
    EXPECT_NE(result.find("a"), std::string::npos);
    EXPECT_NE(result.find("ff"), std::string::npos);
}

// ==================== convertoToUint16 Tests ====================

TEST(UtilConvertoToUint16Test, ConvertsBytes)
{
    nx_data bytes = {0x34, 0x12};
    uint16_t result = Util::convertoToUint16(bytes);
    if (server::Config::getBigEndian())
    {
        EXPECT_EQ(result, 0x3412);
    }
    else
    {
        EXPECT_EQ(result, 0x1234);
    }
}

TEST(UtilConvertoToUint16Test, TooFewBytesThrows)
{
    nx_data bytes = {0x01};
    EXPECT_THROW(Util::convertoToUint16(bytes), std::invalid_argument);
}

// ==================== convertToVector2 Tests ====================

TEST(UtilConvertToVector2Test, ConvertsBytes)
{
    float x = 1.5f, y = 2.5f;
    nx_data bytes(sizeof(float) * 2);
    memcpy(bytes.data(), &x, sizeof(float));
    memcpy(bytes.data() + sizeof(float), &y, sizeof(float));

    Vector2f result = Util::convertToVector2(bytes);
    EXPECT_FLOAT_EQ(result.x, 1.5f);
    EXPECT_FLOAT_EQ(result.y, 2.5f);
}

TEST(UtilConvertToVector2Test, InsufficientBytesThrows)
{
    nx_data bytes = {0x01, 0x02, 0x03};
    EXPECT_THROW(Util::convertToVector2(bytes), std::invalid_argument);
}

// ==================== convertToVector3 Tests ====================

TEST(UtilConvertToVector3Test, ConvertsBytes)
{
    float x = 1.0f, y = 2.0f, z = 3.0f;
    nx_data bytes(sizeof(float) * 3);
    memcpy(bytes.data(), &x, sizeof(float));
    memcpy(bytes.data() + sizeof(float), &y, sizeof(float));
    memcpy(bytes.data() + sizeof(float) * 2, &z, sizeof(float));

    Vector3f result = Util::convertToVector3(bytes);
    EXPECT_FLOAT_EQ(result.x, 1.0f);
    EXPECT_FLOAT_EQ(result.y, 2.0f);
    EXPECT_FLOAT_EQ(result.z, 3.0f);
}

TEST(UtilConvertToVector3Test, InsufficientBytesThrows)
{
    nx_data bytes = {0x01, 0x02};
    EXPECT_THROW(Util::convertToVector3(bytes), std::invalid_argument);
}

// ==================== convertToByteVector Tests ====================

TEST(UtilConvertToByteVectorTest, Uint8)
{
    auto result = Util::convertToByteVector(static_cast<uint8_t>(0xAB));
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 0xAB);
}

TEST(UtilConvertToByteVectorTest, Uint16)
{
    auto result = Util::convertToByteVector(static_cast<uint16_t>(0x1234));
    ASSERT_EQ(result.size(), 2);
    if (server::Config::getBigEndian())
    {
        EXPECT_EQ(result[0], 0x12);
        EXPECT_EQ(result[1], 0x34);
    }
    else
    {
        EXPECT_EQ(result[0], 0x34);
        EXPECT_EQ(result[1], 0x12);
    }
}

TEST(UtilConvertToByteVectorTest, Uint64)
{
    auto result = Util::convertToByteVector(static_cast<uint64_t>(1));
    ASSERT_EQ(result.size(), 8);
    if (server::Config::getBigEndian())
    {
        EXPECT_EQ(result[0], 0);
        EXPECT_EQ(result[7], 1);
    }
    else
    {
        EXPECT_EQ(result[0], 1);
        EXPECT_EQ(result[7], 0);
    }
}

TEST(UtilConvertToByteVectorTest, Float)
{
    float val = 3.14f;
    auto result = Util::convertToByteVector(val);
    ASSERT_EQ(result.size(), sizeof(float));

    float decoded;
    memcpy(&decoded, result.data(), sizeof(float));
    EXPECT_FLOAT_EQ(decoded, 3.14f);
}

TEST(UtilConvertToByteVectorTest, String)
{
    auto result = Util::convertToByteVector(std::string("abc"));
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 'a');
    EXPECT_EQ(result[1], 'b');
    EXPECT_EQ(result[2], 'c');
}

TEST(UtilConvertToByteVectorTest, CharPtr)
{
    const char* str = "test";
    auto result = Util::convertToByteVector(str, 4);
    ASSERT_EQ(result.size(), 4);
    EXPECT_EQ(result[0], 't');
    EXPECT_EQ(result[3], 't');
}

TEST(UtilConvertToByteVectorTest, Vector2f)
{
    Vector2f v(1.0f, 2.0f);
    auto result = Util::convertToByteVector(v);
    ASSERT_EQ(result.size(), sizeof(float) * 2);
}

TEST(UtilConvertToByteVectorTest, Vector3f)
{
    Vector3f v(1.0f, 2.0f, 3.0f);
    auto result = Util::convertToByteVector(v);
    ASSERT_EQ(result.size(), sizeof(float) * 3);
}

TEST(UtilConvertToByteVectorTest, JsonObject)
{
    boost::json::object obj;
    obj["key"] = "value";
    auto result = Util::convertToByteVector(obj);
    EXPECT_FALSE(result.empty());
}

// ==================== uint8PairToUint16 / uint16ToUint8Pair Tests ====================

TEST(UtilPairConversionTest, Uint8PairToUint16)
{
    uint16_t result = Util::uint8PairToUint16(0x34, 0x12);
    EXPECT_EQ(result, 0x1234);
}

TEST(UtilPairConversionTest, Uint8PairToUint16Zero)
{
    uint16_t result = Util::uint8PairToUint16(0x00, 0x00);
    EXPECT_EQ(result, 0x0000);
}

TEST(UtilPairConversionTest, Uint16ToUint8Pair)
{
    uint8_t low, high;
    Util::uint16ToUint8Pair(0x1234, low, high);
    EXPECT_EQ(low, 0x34);
    EXPECT_EQ(high, 0x12);
}

TEST(UtilPairConversionTest, PairRoundtrip)
{
    uint16_t original = 0xABCD;
    uint8_t low, high;
    Util::uint16ToUint8Pair(original, low, high);
    uint16_t result = Util::uint8PairToUint16(low, high);
    EXPECT_EQ(result, original);
}

// ==================== uint64FromFront Tests ====================

TEST(UtilFrontConversionsTest, Uint64FromFront)
{
    nx_data bytes = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF};
    uint64_t result = Util::uint64FromFront(bytes);
    if (server::Config::getBigEndian())
    {
        EXPECT_EQ(result, 0x0100000000000000ULL);
    }
    else
    {
        EXPECT_EQ(result, 0x01ULL);
    }
}

TEST(UtilFrontConversionsTest, Uint64FromFrontTooSmall)
{
    nx_data bytes = {0x01, 0x02, 0x03};
    uint64_t result = Util::uint64FromFront(bytes);
    EXPECT_EQ(result, 0);
}

// ==================== floatFromFront Tests ====================

TEST(UtilFrontConversionsTest, FloatFromFront)
{
    float expected = 3.14f;
    nx_data bytes(sizeof(float) + 4);
    memcpy(bytes.data(), &expected, sizeof(float));
    bytes[sizeof(float)] = 0xFF;

    float result = Util::floatFromFront(bytes);
    EXPECT_FLOAT_EQ(result, 3.14f);
}

TEST(UtilFrontConversionsTest, FloatFromFrontTooSmall)
{
    nx_data bytes = {0x01, 0x02};
    float result = Util::floatFromFront(bytes);
    EXPECT_FLOAT_EQ(result, 0.0f);
}

// ==================== vector2fFromFront Tests ====================

TEST(UtilFrontConversionsTest, Vector2fFromFront)
{
    Vector2f expected(1.5f, 2.5f);
    nx_data bytes(sizeof(float) * 2 + 4);
    memcpy(bytes.data(), &expected.x, sizeof(float));
    memcpy(bytes.data() + sizeof(float), &expected.y, sizeof(float));

    Vector2f result = Util::vector2fFromFront(bytes);
    EXPECT_FLOAT_EQ(result.x, 1.5f);
    EXPECT_FLOAT_EQ(result.y, 2.5f);
}

TEST(UtilFrontConversionsTest, Vector2fFromFrontTooSmall)
{
    nx_data bytes = {0x01, 0x02};
    Vector2f result = Util::vector2fFromFront(bytes);
    EXPECT_FLOAT_EQ(result.x, 0.0f);
    EXPECT_FLOAT_EQ(result.y, 0.0f);
}

// ==================== vector3fFromFront Tests ====================

TEST(UtilFrontConversionsTest, Vector3fFromFront)
{
    Vector3f expected(1.0f, 2.0f, 3.0f);
    nx_data bytes(sizeof(float) * 3 + 4);
    memcpy(bytes.data(), &expected.x, sizeof(float));
    memcpy(bytes.data() + sizeof(float), &expected.y, sizeof(float));
    memcpy(bytes.data() + sizeof(float) * 2, &expected.z, sizeof(float));

    Vector3f result = Util::vector3fFromFront(bytes);
    EXPECT_FLOAT_EQ(result.x, 1.0f);
    EXPECT_FLOAT_EQ(result.y, 2.0f);
    EXPECT_FLOAT_EQ(result.z, 3.0f);
}

TEST(UtilFrontConversionsTest, Vector3fFromFrontTooSmall)
{
    nx_data bytes = {0x01, 0x02, 0x03};
    Vector3f result = Util::vector3fFromFront(bytes);
    EXPECT_FLOAT_EQ(result.x, 0.0f);
    EXPECT_FLOAT_EQ(result.y, 0.0f);
    EXPECT_FLOAT_EQ(result.z, 0.0f);
}

// ==================== removeAmountOfBytesFromVector Tests ====================

TEST(UtilRemoveBytesTest, RemovesBytes)
{
    nx_data bytes = {0x01, 0x02, 0x03, 0x04, 0x05};
    auto result = Util::removeAmountOfBytesFromVector(bytes, 2);
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 0x03);
    EXPECT_EQ(result[1], 0x04);
    EXPECT_EQ(result[2], 0x05);
}

TEST(UtilRemoveBytesTest, RemoveZeroBytes)
{
    nx_data bytes = {0x01, 0x02};
    auto result = Util::removeAmountOfBytesFromVector(bytes, 0);
    EXPECT_EQ(result.size(), 2);
}

TEST(UtilRemoveBytesTest, RemoveMoreThanAvailable)
{
    nx_data bytes = {0x01, 0x02};
    auto result = Util::removeAmountOfBytesFromVector(bytes, 5);
    EXPECT_TRUE(result.empty());
}

TEST(UtilRemoveBytesTest, RemoveAllBytes)
{
    nx_data bytes = {0x01, 0x02, 0x03};
    auto result = Util::removeAmountOfBytesFromVector(bytes, 3);
    EXPECT_TRUE(result.empty());
}

// ==================== createString / createUint64 / createDouble Tests ====================

TEST(UtilJsonHelpersTest, CreateString)
{
    auto val = boost::json::parse(R"({"key": "hello"})");
    std::string result = Util::createString(val, "key");
    EXPECT_EQ(result, "hello");
}

TEST(UtilJsonHelpersTest, CreateStringMissingKey)
{
    auto val = boost::json::parse(R"({"key": "hello"})");
    EXPECT_THROW(Util::createString(val, "missing"), std::runtime_error);
}

TEST(UtilJsonHelpersTest, CreateUint64)
{
    auto val = boost::json::parse(R"({"num": 42})");
    uint64_t result = Util::createUint64(val, "num");
    EXPECT_EQ(result, 42);
}

TEST(UtilJsonHelpersTest, CreateUint64MissingKey)
{
    auto val = boost::json::parse(R"({"num": 42})");
    EXPECT_THROW(Util::createUint64(val, "missing"), std::runtime_error);
}

TEST(UtilJsonHelpersTest, CreateDouble)
{
    auto val = boost::json::parse(R"({"pi": 3.14})");
    double result = Util::createDouble(val, "pi");
    EXPECT_DOUBLE_EQ(result, 3.14);
}

TEST(UtilJsonHelpersTest, CreateDoubleMissingKey)
{
    auto val = boost::json::parse(R"({"pi": 3.14})");
    EXPECT_THROW(Util::createDouble(val, "missing"), std::runtime_error);
}

// ==================== toUint64 Tests ====================

TEST(UtilToUint64Test, FromUint64)
{
    auto val = boost::json::value(uint64_t(42));
    EXPECT_EQ(Util::toUint64(val), 42);
}

TEST(UtilToUint64Test, FromInt64)
{
    auto val = boost::json::value(int64_t(-1));
    EXPECT_EQ(Util::toUint64(val), static_cast<uint64_t>(-1));
}

TEST(UtilToUint64Test, FromStringReturnsZero)
{
    auto val = boost::json::value("hello");
    EXPECT_EQ(Util::toUint64(val), 0);
}

// ==================== Random Functions Tests ====================

TEST(UtilRandomTest, GetRandomUint64NonZero)
{
    uint64_t val = Util::getRandomUint64();
    EXPECT_NE(val, 0u);
}

TEST(UtilRandomTest, GetRandomIntInRange)
{
    for (int i = 0; i < 100; i++)
    {
        int val = Util::getRandomInt(5, 10);
        EXPECT_GE(val, 5);
        EXPECT_LE(val, 10);
    }
}

TEST(UtilRandomTest, GetRandomStringCorrectLength)
{
    std::string s = Util::getRandomString(10);
    EXPECT_EQ(s.size(), 10u);
}

TEST(UtilRandomTest, GetRandomStringAllUppercase)
{
    std::string s = Util::getRandomString(20);
    for (char c : s)
    {
        EXPECT_GE(c, 'A');
        EXPECT_LE(c, 'Z');
    }
}

TEST(UtilRandomTest, GetRandomStringZeroLength)
{
    std::string s = Util::getRandomString(0);
    EXPECT_TRUE(s.empty());
}

// ==================== getDateAndTime Tests ====================

TEST(UtilDateTest, GetDateAndTimeFormat)
{
    std::string dt = Util::getDateAndTime();
    EXPECT_EQ(dt.size(), 19u);
    EXPECT_EQ(dt[4], '-');
    EXPECT_EQ(dt[7], '-');
    EXPECT_EQ(dt[10], '_');
    EXPECT_EQ(dt[13], ':');
    EXPECT_EQ(dt[16], ':');
}

// ==================== getColorMessage Tests ====================

TEST(UtilColorMessageTest, DebugHasWhiteCode)
{
    std::string msg = Util::getColorMessage(logger::LogLevel::Debug, "test");
    EXPECT_NE(msg.find("\033[37m"), std::string::npos);
}

TEST(UtilColorMessageTest, WarningHasYellowCode)
{
    std::string msg = Util::getColorMessage(logger::LogLevel::Warning, "test");
    EXPECT_NE(msg.find("\033[33m"), std::string::npos);
}

TEST(UtilColorMessageTest, ErrorHasRedCode)
{
    std::string msg = Util::getColorMessage(logger::LogLevel::Error, "test");
    EXPECT_NE(msg.find("\033[31m"), std::string::npos);
}

TEST(UtilColorMessageTest, ContainsData)
{
    std::string msg = Util::getColorMessage(logger::LogLevel::Info, "hello world");
    EXPECT_NE(msg.find("hello world"), std::string::npos);
}

// ==================== File Operations Tests ====================

TEST(UtilFileTest, DeleteIfExists)
{
    std::string path = std::tmpnam(nullptr);
    {
        std::ofstream f(path);
        f << "test";
    }
    EXPECT_TRUE(std::filesystem::exists(path));

    Util::deleteIfExists(path);
    EXPECT_FALSE(std::filesystem::exists(path));
}

TEST(UtilFileTest, DeleteIfExistsNonexistent)
{
    EXPECT_NO_THROW(Util::deleteIfExists("/tmp/nonexistent_nexilis_test_file_xyz"));
}

TEST(UtilFileTest, GetNexilisTempPath)
{
    auto path = Util::getNexilisTempPath();
    EXPECT_TRUE(std::filesystem::exists(path));
    EXPECT_NE(path.string().find("nexilis"), std::string::npos);
}

// ==================== Port File Operations Tests ====================

TEST(UtilPortTest, WriteAndReadPort)
{
    auto type = NXProtocol::Type::BOOST_TCP_CLIENT;
    uint16_t port = 12345;

    bool written = Util::writePortToFile(port, type);
    EXPECT_TRUE(written);

    auto readPort = Util::readPortFromFile(type);
    ASSERT_TRUE(readPort.has_value());
    EXPECT_EQ(readPort.value(), port);

    Util::cleanupPortFile(type);
}

TEST(UtilPortTest, ReadPortFromNonexistentFile)
{
    auto type = NXProtocol::Type::AF_UNIX_SOCK_STREAM_SERVER;
    Util::cleanupPortFile(type);

    auto result = Util::readPortFromFile(type);
    EXPECT_FALSE(result.has_value());
}
