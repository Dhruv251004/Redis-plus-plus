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
    src/persistence/AofEntry.cpp
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

add_executable(resp_parser_tests
    tests/resp_parser_tests.cpp
    src/protocol/RespParser.cpp
    src/protocol/RespValue.cpp
)

target_include_directories(resp_parser_tests PRIVATE src)
target_link_libraries(resp_parser_tests PRIVATE GTest::gtest_main)

add_executable(resp_serializer_tests
    tests/resp_serializer_tests.cpp
    src/protocol/RespSerializer.cpp
    src/protocol/RespParser.cpp
    src/protocol/RespValue.cpp
)

target_include_directories(resp_serializer_tests PRIVATE src)
target_link_libraries(resp_serializer_tests PRIVATE GTest::gtest_main)

add_executable(command_executor_tests
    tests/command_executor_tests.cpp
    src/command/CommandExecutor.cpp
    src/network/Connection.cpp
    src/server/ConnectionManager.cpp
    src/pubsub/PubSubManager.cpp
    src/protocol/RespSerializer.cpp
    src/protocol/RespValue.cpp
    src/database/Database.cpp
    src/utility/TimeUtils.cpp
    src/utility/StringUtils.cpp
)

target_include_directories(command_executor_tests PRIVATE src)
target_link_libraries(command_executor_tests PRIVATE GTest::gtest_main spdlog::spdlog)

add_executable(byte_buffer_tests
    tests/byte_buffer_tests.cpp
    src/persistence/ByteBuffer.cpp
)

target_include_directories(byte_buffer_tests PRIVATE src)
target_link_libraries(byte_buffer_tests PRIVATE GTest::gtest_main ZLIB::ZLIB)

add_executable(aof_tests
    tests/aof_tests.cpp
    src/persistence/AOF.cpp
)

target_include_directories(aof_tests PRIVATE src)
target_link_libraries(aof_tests PRIVATE GTest::gtest_main spdlog::spdlog)

add_executable(rdb_tests
    tests/rdb_tests.cpp
    src/persistence/RDB.cpp
    src/persistence/ByteBuffer.cpp
    src/database/Database.cpp
    src/utility/TimeUtils.cpp
    src/utility/Types.cpp
)

target_include_directories(rdb_tests PRIVATE src)
target_link_libraries(rdb_tests PRIVATE GTest::gtest_main ZLIB::ZLIB spdlog::spdlog)

add_executable(aof_entry_tests
    tests/aof_entry_tests.cpp
    src/persistence/AofEntry.cpp
    src/command/CommandExecutor.cpp
    src/network/Connection.cpp
    src/server/ConnectionManager.cpp
    src/pubsub/PubSubManager.cpp
    src/protocol/RespSerializer.cpp
    src/protocol/RespParser.cpp
    src/protocol/RespValue.cpp
    src/database/Database.cpp
    src/utility/TimeUtils.cpp
    src/utility/StringUtils.cpp
)

target_include_directories(aof_entry_tests PRIVATE src)
target_link_libraries(aof_entry_tests PRIVATE GTest::gtest_main spdlog::spdlog)

include(GoogleTest)
gtest_discover_tests(aof_entry_tests)
gtest_discover_tests(byte_buffer_tests)
gtest_discover_tests(aof_tests)
gtest_discover_tests(rdb_tests)
gtest_discover_tests(command_executor_tests)
gtest_discover_tests(resp_serializer_tests)
gtest_discover_tests(db_tests)
gtest_discover_tests(resp_parser_tests)