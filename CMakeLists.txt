cmake_minimum_required(VERSION 3.10.0)
project(game VERSION 0.1.0 LANGUAGES C)

include_directories(external/include)
link_directories(external/lib)
add_executable(game main.c)
target_link_libraries(game raylib m)
