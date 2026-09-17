cmake_minimum_required(VERSION 3.19)

# The Discord bot is a separate CMake project because it pulls in D++ (a large
# dependency) via FetchContent. Build it on demand:
#
#   cmake -S authentication/bot -B authentication/bot/build -G Ninja \
#         -DCMAKE_PREFIX_PATH=<toolchain opt> -DOPENSSL_ROOT_DIR=<toolchain opt>
#   cmake --build authentication/bot/build
#
# The first configure downloads and builds D++ (a few minutes); afterwards it
# is cached.
project(NextGenBot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

include(FetchContent)

# --- D++ (Discord API library) --------------------------------------------
set(DPP_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(BUILD_VOICE_SUPPORT OFF CACHE BOOL "" FORCE) # no opus/sodium needed
set(DPP_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    dpp
    GIT_REPOSITORY https://github.com/brainboxdotcc/DPP.git
    GIT_TAG v10.0.35
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(dpp)

# --- Shared licensing core -------------------------------------------------
# Build the core straight from source so the bot and the rest of the backend
# share one implementation and one database schema.
find_package(OpenSSL REQUIRED)
find_path(SQLITE3_INCLUDE_DIR sqlite3.h)
find_library(SQLITE3_LIBRARY NAMES sqlite3)

add_library(nglicense_bot STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../libnglicense/src/Crypto.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libnglicense/src/License.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libnglicense/src/Database.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libnglicense/src/LicenseService.cpp
)
target_include_directories(nglicense_bot
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../libnglicense/include
    PRIVATE ${SQLITE3_INCLUDE_DIR}
)
target_link_libraries(nglicense_bot PUBLIC OpenSSL::Crypto PRIVATE ${SQLITE3_LIBRARY})

# --- Bot executable --------------------------------------------------------
add_executable(nextgen_bot main.cpp)
target_link_libraries(nextgen_bot PRIVATE nglicense_bot dpp)

if(WIN32)
    target_link_libraries(nextgen_bot PRIVATE ws2_32 crypt32)
endif()
