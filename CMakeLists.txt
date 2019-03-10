cmake_minimum_required(VERSION 2.8.11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
project(gbemu)


add_executable(
  gbemu
  src/main.cpp
  src/emulator.h
  src/emulator.cpp
  include/types.h
  src/cpu.h
  src/cpu.cpp
  src/memory.h
  src/memory.cpp
  src/instruction_table.h
  src/instruction_table.cpp
  src/lcd.h
  src/lcd.cpp
  src/gpu.h
  src/gpu.cpp
  src/pixel.h
  src/renderer.h
  src/sdl_renderer.h
  src/sdl_renderer.cpp
  src/sprite_attribute.h
  src/constants.h
  src/registers/lcdc.h
  src/registers/lcd_stat.h
  src/registers/lyc.h
  src/registers/div.h
  src/registers/tac.h
  src/registers/tima.h
  src/registers/tma.h
  src/registers/interrupt_enabled.h
  src/registers/interrupt_request.h
  src/registers/palette.h
  src/registers/sound.h
  src/input.h
  src/input.cpp
  src/utils.h
  src/sdl_utils.h
  src/timers.h
  src/timers.cpp
  src/sound.h
  src/sound.cpp
  src/square_source.h
  src/square_source.cpp
  src/wave_source.h
  src/wave_source.cpp
  src/noise_source.h
  src/noise_source.cpp
  src/sound_mods/length_mod.h
  src/sound_mods/envelope_mod.h
  src/sound_mods/envelope_mod.cpp
  src/sound_mods/volume_shift_mod.h
  src/sound_mods/volume_shift_mod.cpp
)

find_library(SDL2_LIB NAMES SDL2)
find_library(BOOST_PROGRAM_OPTIONS_LIB NAMES boost_program_options)

target_include_directories(gbemu PRIVATE include)
target_compile_definitions(gbemu PRIVATE span_FEATURE_MEMBER_AT=1)

target_link_libraries(gbemu ${SDL2_LIB} ${BOOST_PROGRAM_OPTIONS_LIB})
target_compile_options(gbemu PRIVATE -Wall -Wextra -pedantic -O3 -g -march=native)
target_link_options(gbemu PRIVATE -fuse-ld=gold)

