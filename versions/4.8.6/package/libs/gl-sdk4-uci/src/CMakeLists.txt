cmake_minimum_required(VERSION 3.0)

project(gl-uci C)

add_definitions(-D_GNU_SOURCE)
add_compile_options(-O -Wall -Werror --std=gnu99 )

add_library(gl-uci SHARED guci.c)
target_link_libraries(gl-uci uci)
set_target_properties(gl-uci PROPERTIES OUTPUT_NAME guci)

install(
    TARGETS gl-uci
    DESTINATION lib
)

install(
    FILES guci.h
    DESTINATION include
)
