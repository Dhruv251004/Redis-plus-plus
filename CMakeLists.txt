cmake_minimum_required(VERSION 3.16)

project(RedisPlusPlus VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ZLIB REQUIRED)

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
    src/database/Database.cpp
    src/utility/TimeUtils.cpp
    src/utility/StringUtils.cpp
    src/utility/redisPlusPlus.cpp
    src/persistence/AOF.cpp
    src/persistence/ByteBuffer.cpp
    src/persistence/RDB.cpp
)

target_include_directories(redispp PRIVATE
    include
    src
)

target_link_libraries(redispp PRIVATE ZLIB::ZLIB)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(redispp PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()