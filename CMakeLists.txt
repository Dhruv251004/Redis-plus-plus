cmake_minimum_required(VERSION 3.16)

project(RedisPlusPlus VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ZLIB REQUIRED)

include(FetchContent)
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.15.3
    FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(spdlog)

add_executable(redispp
    src/main.cpp
    src/network/TcpServer.cpp
    src/network/Connection.cpp
    src/protocol/RespParser.cpp
    src/protocol/RespSerializer.cpp
    src/protocol/RespValue.cpp
    src/command/CommandExecutor.cpp
    src/server/RedisServer.cpp
    src/server/ConnectionManager.cpp
    src/pubsub/PubSubManager.cpp
    src/database/Database.cpp
    src/utility/TimeUtils.cpp
    src/utility/StringUtils.cpp
    src/utility/RedisPlusPlus.cpp
    src/persistence/AOF.cpp
    src/persistence/ByteBuffer.cpp
    src/persistence/RDB.cpp
    src/utility/Types.cpp
)

target_include_directories(redispp PRIVATE
    include
    src
)

target_link_libraries(redispp PRIVATE ZLIB::ZLIB spdlog::spdlog)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(redispp PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()


enable_testing()

find_package(GTest REQUIRED)

add_executable(db_tests
    tests/db_tests.cpp
    src/database/Database.cpp
    src/utility/TimeUtils.cpp
)

target_include_directories(db_tests PRIVATE src)
target_link_libraries(db_tests PRIVATE GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(db_tests)