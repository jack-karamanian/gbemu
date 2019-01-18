cmake_minimum_required(VERSION 2.8.11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
project(gbemu)

add_executable(
  gbemu
  src/main.cpp
  src/cpu.h
  src/cpu.cpp
  src/memory.h
  src/memory.cpp
  src/instruction_table.h
  src/instruction_table.cpp
  src/lcd.h
  src/system.h
  src/system.cpp
)

target_compile_options(gbemu PRIVATE -Wall -Wextra -ggdb)

target_link_libraries(gbemu SDL2)
