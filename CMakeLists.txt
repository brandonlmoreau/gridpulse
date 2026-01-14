cmake_minimum_required(VERSION 3.16)
project(gridpulse VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Fetch dependencies automatically (simpler than vcpkg)
include(FetchContent)

# nlohmann/json - JSON library (fetch first so jwt-cpp can find it)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# cpp-httplib - Header-only HTTP library
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.3
)

# spdlog - Logging library
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
)

# SQLite3 wrapper
FetchContent_Declare(
    SQLiteCpp
    GIT_REPOSITORY https://github.com/SRombauts/SQLiteCpp.git
    GIT_TAG 3.3.1
)

# Find OpenSSL for JWT (before jwt-cpp)
find_package(OpenSSL REQUIRED)

# jwt-cpp - JSON Web Token library
# Tell jwt-cpp to use the nlohmann_json we already fetched
set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JWT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(JWT_EXTERNAL_NLOHMANN_JSON ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    jwt-cpp
    GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
    GIT_TAG v0.7.0
)

FetchContent_MakeAvailable(httplib spdlog SQLiteCpp jwt-cpp)

# Main server library
add_library(gridpulse_lib
    src/database.cpp
    src/api_server.cpp
    src/alert_checker.cpp
    src/auth.cpp
)

target_include_directories(gridpulse_lib PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${jwt-cpp_SOURCE_DIR}/include
)

target_link_libraries(gridpulse_lib PUBLIC
    nlohmann_json::nlohmann_json
    httplib::httplib
    spdlog::spdlog
    SQLiteCpp
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Main server executable
add_executable(gridpulse_server src/main.cpp)
target_link_libraries(gridpulse_server PRIVATE gridpulse_lib)

# Device simulator tool
add_executable(gridpulse_simulator tools/simulator/main.cpp)
target_link_libraries(gridpulse_simulator PRIVATE 
    nlohmann_json::nlohmann_json
    httplib::httplib
    spdlog::spdlog
)
target_include_directories(gridpulse_simulator PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Testing
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    
    add_executable(gridpulse_tests
        tests/test_database.cpp
        tests/test_alerts.cpp
    )
    target_link_libraries(gridpulse_tests PRIVATE 
        gridpulse_lib
        GTest::gtest_main
    )
    
    include(GoogleTest)
    gtest_discover_tests(gridpulse_tests)
endif()
