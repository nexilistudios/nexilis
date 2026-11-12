#     Copyright (C) 2026 Valtteri Viirret
#     This file is part of the Nexilis Project.
#
#     This file is free software: you can redistribute it and/or modify
#     it under the terms of the GNU Lesser General Public License as
#     published by the Free Software Foundation, either version 3 of the
#     License, or (at your option) any later version.
#
#     This file is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU Lesser General Public License for more details.
#
#     You should have received a copy of the GNU Lesser General Public License
#     along with this file.  If not, see <https://gnu.org>.

cmake_minimum_required(VERSION 3.25)
project(nexilis_tests)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

if (NEXILIS_IS_NIX)
    list(APPEND CMAKE_PREFIX_PATH "ENV{out}")
else()
    list(APPEND CMAKE_PREFIX_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/../../nexilis/build")
endif()

message(STATUS "Nexilis paths: ${CMAKE_PREFIX_PATH}")

find_package(Nexilis REQUIRED CONFIG)
find_package(Boost 1.75 REQUIRED COMPONENTS json)
include(../ColorizedStatus.cmake)
print_nexilis_status()

find_package(GTest REQUIRED)

file(GLOB_RECURSE NEXILIS_TEST_SOURCES CONFIGURE_DEPENDS "src/*.cc")

add_executable(nexilis_tests ${NEXILIS_TEST_SOURCES})

target_link_libraries(nexilis_tests PRIVATE
    Nexilis::nexilis
    GTest::GTest
    Boost::json
)

# Only add include dirs if not using the config file
if(NOT TARGET Nexilis::nexilis)
    target_include_directories(nexilis_tests PRIVATE
        ${CMAKE_PREFIX_PATH}/include
    )
endif()

install(TARGETS nexilis_tests DESTINATION bin)

