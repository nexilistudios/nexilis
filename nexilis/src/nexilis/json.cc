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

#include <nexilis/json.hh>

#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>

#include <fstream>
#include <iostream>
#include <stdexcept>

namespace nexilis
{

// Create JSON data from given key-value pairs
boost::json::object Json::createJSON(const std::map<std::string, boost::json::value>& keyValues)
{
    boost::json::object json_obj;
    for (const auto& kv : keyValues)
    {
        json_obj[kv.first] = kv.second;
    }
    return json_obj;
}

void Json::emplace(boost::json::object& first, const boost::json::object& second)
{
    for (const auto& [key, value] : second)
    {
        first.emplace(key, value);
    }
}

boost::json::object Json::convertToJSON(const nx_data& bytes)
{
    if (bytes.size() > NEXILIS_MAX_JSON_MESSAGE)
        throw std::length_error("Server JSON message exceeds the size limit");

    std::string jsonString(bytes.begin(), bytes.end());
    boost::json::parse_options options;
    options.max_depth = 32;
    return boost::json::parse(jsonString, {}, options).as_object();
}

/// Read JSON data from file.
boost::json::value Json::readJSONFromFile(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        Log::error("Error: Unable to open file ", filename);
        return boost::json::value();
    }

    std::string json_str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    boost::system::error_code ec;
    boost::json::value json_value = boost::json::parse(json_str, ec);
    if (ec)
    {
        Log::error("Error parsing JSON: ", ec.message());
        return boost::json::value();
    }
    return json_value;
}

void Json::saveToFile(const boost::json::object& obj, const std::string& filePath)
{
    std::ofstream file(filePath.c_str());
    if (!file.is_open())
    {
        Log::error("Failed to create a file at path: ", filePath);
    }
    file << boost::json::serialize(obj);
    file.close();
}

void Json::print(const boost::json::object& obj)
{
    for (const auto& [key, value] : obj)
    {
        std::cout << key << ": ";

        if (value.is_string())
        {
            std::cout << value.as_string();
        }
        else if (value.is_int64())
        {
            std::cout << value.as_int64();
        }
        else if (value.is_uint64())
        {
            std::cout << value.as_uint64();
        }
        else if (value.is_object())
        {
            // If the value is another object, recursively print it
            print(value.as_object());
        }
        // Very dirty hacks
        else if (value.is_array())
        {
            for (const auto& item : value.as_array())
            {
                if (item.is_string())
                {
                    std::cout << item.as_string();
                }
                else if (item.is_number())
                {
                    std::cout << item.as_int64();
                }
            }
        }
        else
        {
            Log::info("key: ", key);
            Log::info("value: ", value);
            Log::error("Unsupported value type");
        }
        std::cout << std::endl;
    }
}

std::string Json::toString(const boost::json::object& obj)
{
    std::string result;
    for (const auto& [key, value] : obj)
    {
        result += std::string(key.data(), key.size()) + ": ";

        if (value.is_string())
        {
            result += value.as_string();
        }
        else if (value.is_int64())
        {
            result += std::to_string(value.as_int64());
        }
        else if (value.is_uint64())
        {
            result += std::to_string(value.as_uint64());
        }
        else if (value.is_object())
        {
            result += toString(value.as_object());
        }
        else if (value.is_array())
        {
            for (const auto& item : value.as_array())
            {
                if (item.is_string())
                {
                    result += item.as_string();
                }
                else if (item.is_number())
                {
                    result += std::to_string(item.as_int64());
                }
            }
        }
        else
        {
            // Handle unsupported value type
            result += "Unsupported value type";
        }
        result += "\n";
    }
    return result;
}

} // namespace nexilis
