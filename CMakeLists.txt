cmake_minimum_required(VERSION 3.21)
project(broadcast_c C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(broadcast_c
        main.c)
