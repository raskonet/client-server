cmake_minimum_required(VERSION 3.10)

project(LicenseManager C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_executable(server server.c)
target_link_libraries(server PRIVATE Threads::Threads)

add_executable(client client.c)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(server PRIVATE -Wall -Wextra)
    target_compile_options(client PRIVATE -Wall -Wextra)
endif()

set_target_properties(server client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
