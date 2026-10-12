cmake_minimum_required(VERSION 3.25)
project(nexilis)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# VERSION.txt versioning (relative to this library so add_subdirectory works too)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION.txt" PROJECT_VERSION_RAW)
string(STRIP "${PROJECT_VERSION_RAW}" PROJECT_VERSION_RAW)
string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)(-[a-zA-Z0-9]+)?" VERSION_PARTS "${PROJECT_VERSION_RAW}")

set(NEXILIS_VERSION_MAJOR ${CMAKE_MATCH_1})
set(NEXILIS_VERSION_MINOR ${CMAKE_MATCH_2})
set(NEXILIS_VERSION_PATCH ${CMAKE_MATCH_3})
set(NEXILIS_VERSION_SUFFIX ${CMAKE_MATCH_4})
set(NEXILIS_VERSION "${NEXILIS_VERSION_MAJOR}.${NEXILIS_VERSION_MINOR}.${NEXILIS_VERSION_PATCH}${NEXILIS_VERSION_SUFFIX}")
set(NEXILIS_VERSION_FULL "${NEXILIS_VERSION}")

message(STATUS "System Information:")
message(STATUS "  OS: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Arch: ${CMAKE_HOST_SYSTEM_PROCESSOR}")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "  C++ Compiler: ${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")

# Setting default if not specified via -DCMAKE_BUILD_TYPE.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type (Debug or Release)" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(BUILD_SHARED_LIBS ON)

# Used for the optional transport TLS (TLS-PSK) and the end-to-end message
# encryption (PBKDF2 + AES-256-GCM).
find_package(OpenSSL 1.1.1 REQUIRED)

function(configure_build_environment)
    # Nix devshell.
    if(DEFINED ENV{NIX_BUILD_CORES})
        set(USE_SUBMODULES 1 PARENT_SCOPE)
        set(IS_NIX 1 PARENT_SCOPE)
        set(IS_LOCAL 0 PARENT_SCOPE)
        set(IS_CI 0 PARENT_SCOPE)
        return()
    endif()

    set(CI_VARS
        "GITHUB_ACTIONS"
        "GITLAB_CI"
        "CI"
        "TRAVIS"
        "CIRCLECI"
    )

    # Check for CI.
    foreach(var ${CI_VARS})
        if(DEFINED ENV{${var}})
            set(USE_SUBMODULES 0 PARENT_SCOPE)
            set(IS_CI 1 PARENT_SCOPE)
            set(IS_LOCAL 0 PARENT_SCOPE)
            set(IS_NIX 0 PARENT_SCOPE)
            return()
        endif()
    endforeach()

    # Use submodules locally.
    set(USE_SUBMODULES 1 PARENT_SCOPE)
    set(IS_LOCAL 1 PARENT_SCOPE)
    set(IS_CI 0 PARENT_SCOPE)
    set(IS_NIX 0 PARENT_SCOPE)
endfunction()

configure_build_environment()

set(NEXILIS_IS_CI ${IS_CI} CACHE BOOL "Whether this is a CI build" FORCE)
set(NEXILIS_IS_NIX ${IS_NIX} CACHE BOOL "Whether this is a Nix build" FORCE)
set(NEXILIS_IS_LOCAL ${IS_LOCAL} CACHE BOOL "Whether this is a local build" FORCE)
set(NEXILIS_USE_SUBMODULES ${USE_SUBMODULES} CACHE BOOL "Whether to use submodules" FORCE)

message(STATUS "Build configuration:")
message(STATUS "  CI Environment: ${NEXILIS_IS_CI}")
message(STATUS "  Nix Build: ${NEXILIS_IS_NIX}")
message(STATUS "  Local Build: ${NEXILIS_IS_LOCAL}")
message(STATUS "  Use Submodules: ${NEXILIS_USE_SUBMODULES}")

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if (MSVC)
        add_compile_options(/W4 /WX)
    endif()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_compile_options(-static-libstdc++ -static-libgcc -pthread)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_compile_options(-Wall -Wextra -pedantic -pthread)
endif()

add_compile_definitions(
    NEXILIS_IS_CI=${IS_CI}
    NEXILIS_IS_NIX=${IS_NIX}
    NEXILIS_IS_LOCAL=${IS_LOCAL}
    NEXILIS_USE_SUBMODULES=${USE_SUBMODULES}
)

if (USE_SUBMODULES)
    message(STATUS "Using boost submodules")
    set(BOOST_SUBMODULE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third-party/boost")

    # Libraries to build (json and its dependencies)
    set(BOOST_LIBS_TO_BUILD
        # Core dependencies (order matters for some)
        assert config core mp11 throw_exception static_assert
        predef winapi align move describe endian variant2
        intrusive container_hash compat
        # Libraries with source files
        container system json
    )

    # Glob ALL include directories for headers
    file(GLOB BOOST_LIB_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/third-party/boost/libs/*")
    set(NEXILIS_BOOST_INCLUDE_DIRS "")
    foreach(lib_dir ${BOOST_LIB_DIRS})
        set(lib_include_dir "${lib_dir}/include")
        if(EXISTS ${lib_include_dir})
            list(APPEND NEXILIS_BOOST_INCLUDE_DIRS "${lib_include_dir}")
        endif()
    endforeach()

    # Build only the libraries we need
    foreach(lib ${BOOST_LIBS_TO_BUILD})
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third-party/boost/libs/${lib}/CMakeLists.txt)
           add_subdirectory(third-party/boost/libs/${lib} SYSTEM)
        endif()
    endforeach()

    set(NEXILIS_BOOST_INCLUDE_DIRS "${NEXILIS_BOOST_INCLUDE_DIRS}" CACHE STRING "Boost include directories for submodules" FORCE)
else()
    message(STATUS "Using system boost")
    find_package(Boost 1.75 REQUIRED COMPONENTS json)

    if(NOT Boost_FOUND)
        message(FATAL_ERROR, "System boost not found and submodules missing")
    endif()

    # Create imported targets for system Boost
    if(NOT TARGET Boost::json)
        if (NOT TARGET BOOST::json)
            add_library(Boost::json ALIAS Boost::boost_json)
        else()
            add_library(Boost::json UNKNOWN IMPORTED)
            set_target_properties(Boost::json PROPERTIES
                IMPORTED_LOCATION "${Boost_JSON_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
                INTERFACE_LINK_LIBRARIES "Boost::headers"
            )
        endif()
    endif()
endif()


file(GLOB_RECURSE NEXILIS_SOURCES
    CONFIGURE_DEPENDS
    "src/nexilis/*.cc"
)

# The native socket backends use POSIX headers and socket calls. Windows uses
# the Boost.Asio TCP and UDP backends instead.
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(FILTER NEXILIS_SOURCES EXCLUDE REGEX "/protocol/af_(inet|unix)/[^/]+\\.cc$")
endif()

file (GLOB_RECURSE NEXILISC_SOURCES
    CONFIGURE_DEPENDS
    "src/nexilisc/*.cc"
)

add_library(nexilis SHARED ${NEXILIS_SOURCES})
add_library(nexilisc SHARED ${NEXILISC_SOURCES})

# Keep sibling libraries discoverable after a Unity build copies them into
# build_Data/Plugins, without referring back to this machine's build directory.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_target_properties(nexilis nexilisc PROPERTIES
        BUILD_RPATH_USE_ORIGIN TRUE
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN")
endif()

target_link_libraries(nexilisc PUBLIC nexilis)

target_include_directories(nexilis PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_include_directories(nexilisc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(nexilis PUBLIC
    $<IF:$<BOOL:${USE_SUBMODULES}>,boost_json,Boost::json>
)

target_link_libraries(nexilis PUBLIC OpenSSL::SSL)

target_link_libraries(nexilisc PUBLIC
    $<IF:$<BOOL:${USE_SUBMODULES}>,boost_json,Boost::json>
)

target_link_libraries(nexilisc PUBLIC OpenSSL::SSL)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(nexilis PRIVATE
        ws2_32
        mswsock
    )
    target_link_libraries(nexilisc PRIVATE
        ws2_32
        mswsock
    )
endif()

install(TARGETS nexilis nexilisc
    EXPORT NexilisTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

export(EXPORT NexilisTargets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/NexilisTargets.cmake"
    NAMESPACE Nexilis::
)

install(EXPORT NexilisTargets
    FILE NexilisTargets.cmake
    NAMESPACE Nexilis::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Nexilis
)

configure_package_config_file(
    cmake/NexilisConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/NexilisConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Nexilis
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/NexilisConfigVersion.cmake
    VERSION ${NEXILIS_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/NexilisConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/NexilisConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Nexilis
)
