cmake_minimum_required(VERSION 2.8.11)
project(gbemu)

add_executable(
  gbemu
  src/main.c
)

target_link_libraries(gbemu SDL2)
