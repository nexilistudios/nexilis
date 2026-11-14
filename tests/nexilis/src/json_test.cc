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

#include <nexilis/json.hh>

#include <boost/json/parse.hpp>

#include <stdexcept>

using namespace nexilis;

TEST(JsonTest, CreateJSONFromMap)
{
    std::map<std::string, boost::json::value> kv;
    kv["name"] = "test";
    kv["count"] = 42;

    auto obj = Json::createJSON(kv);

    EXPECT_EQ(obj.at("name").as_string(), "test");
    EXPECT_EQ(obj.at("count").as_int64(), 42);
}

TEST(JsonTest, CreateJSONEmpty)
{
    std::map<std::string, boost::json::value> kv;
    auto obj = Json::createJSON(kv);
    EXPECT_TRUE(obj.empty());
}

TEST(JsonTest, ConvertToJSONFromBytes)
{
    std::string jsonStr = R"({"key":"value","num":10})";
    nx_data bytes(jsonStr.begin(), jsonStr.end());

    auto obj = Json::convertToJSON(bytes);

    EXPECT_EQ(obj.at("key").as_string(), "value");
    EXPECT_EQ(obj.at("num").as_int64(), 10);
}

TEST(JsonTest, ConvertToJSONRejectsOversizedMessage)
{
    nx_data bytes(NEXILIS_MAX_JSON_MESSAGE + 1, ' ');
    EXPECT_THROW(Json::convertToJSON(bytes), std::length_error);
}

TEST(JsonTest, ConvertToJSONRejectsExcessiveNesting)
{
    std::string jsonStr(33, '[');
    jsonStr += "0";
    jsonStr.append(33, ']');
    nx_data bytes(jsonStr.begin(), jsonStr.end());
    EXPECT_ANY_THROW(Json::convertToJSON(bytes));
}

TEST(JsonTest, ToString)
{
    boost::json::object obj;
    obj["hello"] = "world";
    obj["number"] = 5;

    std::string result = Json::toString(obj);

    EXPECT_NE(result.find("hello"), std::string::npos);
    EXPECT_NE(result.find("world"), std::string::npos);
    EXPECT_NE(result.find("5"), std::string::npos);
}

TEST(JsonTest, EmplaceMergesObjects)
{
    boost::json::object first;
    first["a"] = 1;

    boost::json::object second;
    second["b"] = 2;

    Json::emplace(first, second);

    EXPECT_EQ(first.at("a").as_int64(), 1);
    EXPECT_EQ(first.at("b").as_int64(), 2);
}

TEST(JsonTest, EmplaceOverlappingKeysKeepsFirst)
{
    boost::json::object first;
    first["key"] = "original";

    boost::json::object second;
    second["key"] = "new";

    Json::emplace(first, second);

    EXPECT_EQ(first.at("key").as_string(), "original");
}

TEST(JsonTest, SaveAndReadJSONFile)
{
    boost::json::object obj;
    obj["test"] = "roundtrip";
    obj["val"] = 99;

    std::string path = std::tmpnam(nullptr);
    path += ".json";

    Json::saveToFile(obj, path);

    auto value = Json::readJSONFromFile(path);
    auto readObj = value.as_object();

    EXPECT_EQ(readObj.at("test").as_string(), "roundtrip");
    EXPECT_EQ(readObj.at("val").as_int64(), 99);

    std::remove(path.c_str());
}

TEST(JsonTest, ReadJSONFromFileNonexistent)
{
    auto value = Json::readJSONFromFile("/tmp/nonexistent_nexilis_test_file_12345.json");
    EXPECT_TRUE(value.is_null());
}
