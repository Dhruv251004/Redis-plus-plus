cmake_minimum_required(VERSION 3.16)

project(RedisPlusPlus VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(redispp
    src/main.cpp
    src/network/TcpServer.cpp
)

target_include_directories(redispp PRIVATE
    include
    src
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(redispp PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()