cmake_minimum_required(VERSION 3.16)

add_library(vm32legacy STATIC
    src/vm32.c
    src/funcs.c
    src/timers.c
    src/hashing.c
    src/debug.c
    src/cleanup.c
    src/loader.c
    src/hot_reload.c
    src/vm_helpers.c
    src/vm_lifecycle.c
    src/vm_runtime.c
    src/vm_tables.c
)

target_include_directories(vm32legacy
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(vm32legacy PUBLIC m)
