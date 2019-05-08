cmake_minimum_required(VERSION 3.8)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(gbemu_core)

include(CTest)

find_package(SDL2 CONFIG REQUIRED)
find_package(doctest CONFIG REQUIRED)

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
)


add_library(${PROJECT_NAME} ${SRCS})
#add_test(${PROJECT_NAME}_test gbemu --exit)

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)


target_include_directories(${PROJECT_NAME}
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

#target_compile_definitions(${PROJECT_NAME} PRIVATE span_FEATURE_MEMBER_AT=1 BOOST_RESULT_OF_USE_DECLTYPE=1)
target_compile_definitions(${PROJECT_NAME} PRIVATE
  span_FEATURE_MEMBER_AT=1
  span_CONFIG_CONTRACT_VIOLATION_THROWS
  BOOST_RESULT_OF_USE_DECLTYPE=1
  _GLIBCXX_DEBUG
)

#target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_core doctest::doctest ${BOOST_PROGRAM_OPTIONS_LIB})
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2 doctest::doctest ${GBEMU_CORE_EXTRA_LIBS})

target_compile_options(${PROJECT_NAME} PRIVATE
 -Wall
 -Wextra
 -Wshadow
 -Wpedantic
 -Wdouble-promotion
 -Weffc++
 -ggdb3
 )

if (NOT ANDROID)
  target_link_options(${PROJECT_NAME} PRIVATE -march=native)
endif()
#target_link_options(${PROJECT_NAME} PRIVATE -fuse-ld=gold)
#target_link_options(${PROJECT_NAME}_core PRIVATE -fuse-ld=gold)

