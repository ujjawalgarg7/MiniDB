cmake_minimum_required(VERSION 3.16)

project(MiniDB C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
)

find_package(Threads REQUIRED)


# ============================================================
# Main MiniDB server
# ============================================================

add_executable(
        MiniDB

        src/main.c
        src/database.c
        src/server.c
        src/thread_pool.c
        src/wal.c
        src/persistence.c
)

target_link_libraries(
        MiniDB
        PRIVATE
        Threads::Threads
)


# ============================================================
# Expiration test
# ============================================================

add_executable(
        expiration_test

        tests/expiration_test.c
        src/database.c
)

target_link_libraries(
        expiration_test
        PRIVATE
        Threads::Threads
)


# ============================================================
# Expiration stress test
# ============================================================

add_executable(
        expiration_stress_test

        tests/expiration_stress_test.c
        src/database.c
)

target_link_libraries(
        expiration_stress_test
        PRIVATE
        Threads::Threads
)


# ============================================================
# Persistence test
# ============================================================

add_executable(
        persistence_test

        tests/persistence_test.c
        src/database.c
        src/persistence.c
        src/wal.c
)

target_link_libraries(
        persistence_test
        PRIVATE
        Threads::Threads
)


# ============================================================
# WAL test
# ============================================================

add_executable(
        wal_test

        tests/wal_test.c
        src/database.c
        src/wal.c
)

target_link_libraries(
        wal_test
        PRIVATE
        Threads::Threads
)

enable_testing()

add_test(NAME wal_test
        COMMAND wal_test)

add_test(NAME persistence_test
        COMMAND persistence_test)

add_test(NAME expiration_test
        COMMAND expiration_test)

add_test(NAME expiration_stress_test
        COMMAND expiration_stress_test)

add_test(NAME compaction_test
        COMMAND compaction_test)


# ============================================================
# Compaction test
# ============================================================

add_executable(
        compaction_test

        tests/compaction_test.c
        src/database.c
        src/persistence.c
        src/wal.c
)

target_link_libraries(
        compaction_test
        PRIVATE
        Threads::Threads
)