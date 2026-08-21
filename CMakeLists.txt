cmake_minimum_required(VERSION 3.20)

project(MiniDB C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -g")

add_executable(MiniDB
        src/main.c
        src/database.c
        src/server.c
        src/thread_pool.c
)

target_link_libraries(MiniDB
        pthread
)

add_executable(stress_test
        tests/stress_test.c
)

target_link_libraries(stress_test
        pthread
)