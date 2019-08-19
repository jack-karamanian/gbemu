cmake_minimum_required(VERSION 3.8)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(gbemu_core)

include(CTest)

if (NOT EMSCRIPTEN)
find_package(SDL2  CONFIG REQUIRED)
  find_package(doctest CONFIG REQUIRED)
  find_package(range-v3 CONFIG REQUIRED)
  find_package(fmt CONFIG REQUIRED)
  find_package(benchmark CONFIG REQUIRED)
endif()

set(SRCS
  include/emulator.h
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
  src/color.h
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
  src/registers/dmg.h
  src/registers/cgb.h
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
  src/mbc.h
  src/mbc.cpp
  src/task.h
  src/rom_loader.h
  src/rom_loader.cpp
  src/error_handling.h
  src/error_handling.cpp
  src/gba/cpu.h
  src/gba/cpu.cpp
  src/gba/mmu.h
  src/gba/mmu.cpp
  src/gba/lcd.h
  src/gba/lcd.cpp
  src/gba/input.h
  src/gba/thumb_to_arm.h
  src/gba/thumb_to_arm.cpp
  src/gba/assembler.h
  src/gba/assembler.cpp
  src/gba/hardware.h
  src/gba/emulator.h
  src/gba/emulator.cpp
  src/gba/dma.h
  src/gba/dma.cpp
  src/gba/io_registers.h
  src/gba/io_registers.cpp
  src/gba/timer.h
  src/gba/timer.cpp
  src/gba/gpu.h
  src/gba/gpu.cpp
)


add_library(${PROJECT_NAME} ${SRCS})

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

target_include_directories(${PROJECT_NAME}
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../external/span-lite/include>
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  span_FEATURE_MEMBER_AT=1
  BOOST_RESULT_OF_USE_DECLTYPE=1
)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
  SDL2::SDL2-static
  doctest::doctest
  fmt::fmt
  fmt::fmt-header-only
  range-v3
  ${GBEMU_CORE_EXTRA_LIBS}
)

target_compile_options(${PROJECT_NAME} PRIVATE
 -Wall
 -Wextra
 -Wshadow
 -Wpedantic
 -Wdouble-promotion
 -Wunused
 -Wold-style-cast
 -ggdb3
 )

if (NOT ANDROID)
  target_link_options(${PROJECT_NAME} PRIVATE -march=native)
endif()

