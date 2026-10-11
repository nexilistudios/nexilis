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

# MinGW-w64 Cross-Compilation Toolchain
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Compilers
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Sysroot
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
include_directories(${CMAKE_FIND_ROOT_PATH}/include)

# Search behavior
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Compiler and linker settings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -pthread")
set(CMAKE_SHARED_LINKER_FLAGS "-lwinpthread -Wl,--enable-auto-import")

add_compile_definitions(
    NOMINMAX
    _WIN32_WINT=0x0A00
    BOOST_ASIO_DISABLE_LOCAL_SOCKETS
    BOOST_ASIO_DISABLE_BOOST_DATE_TIME=1
    BOOST_ASIO_DISABLE_BOOST_REGEX=1
    BOOST_ASIO_HAS_STD_CHRONO=1
)

# Boost config
set(BOOST_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third-party/boost)
set(Boost_NO_SYSTEM_PATHS ON)

# Skip compiler checks.
set(CMAKE_CXX_COMPILER_WORKS 1)
set(CMAKE_C_COMPILER_WORKS 1)

message(STATUS "Cross-compiling for Windows using ${CMAKE_CXX_COMPILER}")
