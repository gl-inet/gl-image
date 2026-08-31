cmake_minimum_required(VERSION 3.0)

project(gl-sdk4-s2s C)

add_library(gl-sdk4-s2s MODULE s2s.c wireguard.c)
target_compile_options(gl-sdk4-s2s PRIVATE -O -Wall -Werror --std=gnu99)
set_target_properties(gl-sdk4-s2s PROPERTIES OUTPUT_NAME s2s PREFIX "")
target_link_libraries(gl-sdk4-s2s PRIVATE jansson guci gl-utils)

add_executable(echo_server echo_server.c)
target_link_libraries(echo_server ubox)

install(
    TARGETS gl-sdk4-s2s echo_server
    DESTINATION usr/bin
    DESTINATION lib/oui-httpd/rpc
)
