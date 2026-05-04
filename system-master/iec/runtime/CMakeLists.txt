cmake_minimum_required(VERSION 3.16)

add_subdirectory(legacy)

add_library(vm32runtime STATIC
    VM32Engine.cpp
    VM32Component.cpp
    logger_bridge.cpp
)

target_include_directories(vm32runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/legacy/include
)

target_link_libraries(vm32runtime
    PUBLIC
        vm32legacy
)
