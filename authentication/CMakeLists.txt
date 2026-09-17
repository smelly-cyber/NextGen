cmake_minimum_required(VERSION 3.19)

project(NextGenAuth
    VERSION 1.0.0
    DESCRIPTION "NextGen Tweaks licensing core, CLI and validation server"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# --- Dependencies ----------------------------------------------------------
# OpenSSL (Ed25519 / SHA-256 / PBKDF2) and SQLite ship with the Qt mingw
# toolchain under Tools/mingw1310_64/opt; a stock system install works too.
find_package(OpenSSL REQUIRED)

find_path(SQLITE3_INCLUDE_DIR sqlite3.h)
find_library(SQLITE3_LIBRARY NAMES sqlite3)
if(NOT SQLITE3_INCLUDE_DIR OR NOT SQLITE3_LIBRARY)
    message(FATAL_ERROR "sqlite3 not found. Point CMAKE_PREFIX_PATH at your toolchain's opt dir.")
endif()

# --- Shared licensing core -------------------------------------------------
add_library(nglicense STATIC
    libnglicense/src/Crypto.cpp
    libnglicense/src/License.cpp
    libnglicense/src/Database.cpp
    libnglicense/src/LicenseService.cpp
)
target_include_directories(nglicense
    PUBLIC libnglicense/include
    PRIVATE ${SQLITE3_INCLUDE_DIR}
)
target_link_libraries(nglicense PUBLIC OpenSSL::Crypto PRIVATE ${SQLITE3_LIBRARY})

if(WIN32)
    target_link_libraries(nglicense PUBLIC ws2_32 crypt32)
endif()

# --- Command line tool -----------------------------------------------------
add_executable(ngtlicense cli/main.cpp)
target_link_libraries(ngtlicense PRIVATE nglicense)

# --- Self test -------------------------------------------------------------
enable_testing()
add_executable(nglicense_selftest libnglicense/tests/selftest.cpp)
target_link_libraries(nglicense_selftest PRIVATE nglicense)
add_test(NAME nglicense_selftest COMMAND nglicense_selftest)

# --- Validation + account server (added once httplib is fetched) -----------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/server/main.cpp")
    add_subdirectory(server)
endif()
