cmake_minimum_required(VERSION 3.15)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(gbemu_core)

include(CTest)

set(GBEMU_DISABLE_TESTS OFF CACHE STRING "Disable tests")
set(GBEMU_DISABLE_BOUNDS_CHECKS ON CACHE STRING "Disable bounds checking") 
set(GBEMU_ENABLE_LTO OFF CACHE BOOL "Enables LTO")

if (NOT EMSCRIPTEN)
  find_package(SDL2 CONFIG REQUIRED)
  find_package(doctest CONFIG REQUIRED)
  find_package(fmt CONFIG REQUIRED)
  find_package(benchmark CONFIG REQUIRED)
  find_package(span-lite  CONFIG REQUIRED)
  find_package(range-v3 CONFIG REQUIRED)
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
  src/gba/hle.h
  src/gba/hle.cpp
  src/static_vector.h
  src/gba/sound.h
  src/gba/sound.cpp
  src/gba/interrupts.h
  src/gba/interrupts.cpp
  src/gba/assembler.h
  src/gba/assembler.cpp
)

add_library(${PROJECT_NAME} ${SRCS})

set_target_properties(${PROJECT_NAME} PROPERTIES
  CXX_STANDARD 17
  INTERPROCEDURAL_OPTIMIZATION ${GBEMU_ENABLE_LTO}
)

target_include_directories(${PROJECT_NAME}
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  span_FEATURE_MEMBER_AT=1
  span_CONFIG_CONTRACT_VIOLATION_THROWS=1
  $<$<BOOL:${GBEMU_DISABLE_TESTS}>:DOCTEST_CONFIG_DISABLE=1>
  $<$<BOOL:${GBEMU_DISABLE_BOUNDS_CHECKS}>:span_CONFIG_CONTRACT_LEVEL_OFF=1>
)

if (NOT EMSCRIPTEN)
  target_link_libraries(${PROJECT_NAME}
    PUBLIC
    SDL2::SDL2-static
    doctest::doctest
    fmt::fmt
    nonstd::span-lite
    range-v3
    concepts
    meta
  )
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_SDL=2 -s LEGACY_GL_EMULATION=1 -s FORCE_FILESYSTEM=1 -s ASYNCIFY=1")
endif()

target_compile_options(${PROJECT_NAME} PRIVATE
#-Wall
# -Wextra
# -Wshadow
# -Wpedantic
# -Wdouble-promotion
# -Wunused
# -g
# -pipe
# $<$<CONFIG:Release>:-O3>
)

if (TRUE )
  add_executable(gbemu_benchmark
    src/gba/benchmark/mmu.cpp
  )

  set_target_properties(gbemu_benchmark PROPERTIES
    CXX_STANDARD 17
    INTERPROCEDURAL_OPTIMIZATION FALSE
  MSVC_RUNTIME_LIBRARY MultiThreaded
  )

  target_link_libraries(gbemu_benchmark PUBLIC
    ${PROJECT_NAME}
    doctest::doctest
    benchmark::benchmark
  )
  target_compile_definitions(gbemu_benchmark PRIVATE
    span_FEATURE_MEMBER_AT=1
    BOOST_RESULT_OF_USE_DECLTYPE=1
  )

  if (NOT ANDROID)
    target_link_options(${PROJECT_NAME} PUBLIC
    )
    target_link_options(gbemu_benchmark PRIVATE)
  endif()
endif()

add_library(${PROJECT_NAME}_libretro SHARED src/libretro.cpp)
target_link_libraries(${PROJECT_NAME}_libretro PRIVATE ${PROJECT_NAME})

