cmake_minimum_required(VERSION 3.20)

project(MiniDB C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(MiniDB
        src/main.c
        src/database.c
        src/server.c
        src/thread_pool.c
)

target_link_libraries(MiniDB
        pthread
)

add_executable(expiration_stress_test
        tests/expiration_stress_test.c
        src/database.c
)

target_link_libraries(expiration_stress_test
        pthread
)

add_executable(expiration_test
        tests/expiration_test.c
        src/database.c
        tests/expiration_stress_test.c
)

target_link_libraries(expiration_test
        pthread
)

add_executable(stress_test
        tests/stress_test.c
        src/database.c
)

target_link_libraries(stress_test
        pthread
)