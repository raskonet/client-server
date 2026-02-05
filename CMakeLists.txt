cmake_minimum_required(VERSION 3.10)

project(LicenseManager LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

set(COMMON_WARNINGS -Wall -Wextra -Werror)

add_executable(server server.c)
target_compile_options(server PRIVATE ${COMMON_WARNINGS})
target_link_libraries(server PRIVATE Threads::Threads)

add_executable(client client.c)
target_compile_options(client PRIVATE ${COMMON_WARNINGS})
target_link_libraries(client PRIVATE Threads::Threads)

add_executable(admin admin.c)
target_compile_options(admin PRIVATE ${COMMON_WARNINGS})
target_link_libraries(admin PRIVATE Threads::Threads)

# output folder
set_target_properties(server client admin PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")

