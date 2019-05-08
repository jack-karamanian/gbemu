cmake_minimum_required(VERSION 3.8)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(gbemu)

set(GBEMU_CORE_EXTRA_LIBS
  ${DEVKITPRO}/portlibs/switch/lib/libEGL.a
  ${DEVKITPRO}/portlibs/switch/lib/libglapi.a
  ${DEVKITPRO}/portlibs/switch/lib/libdrm_nouveau.a
  ${DEVKITPRO}/portlibs/switch/lib/libGLESv2.a
  ${DEVKITPRO}/libnx/lib/libnx.a
)

add_subdirectory(${CMAKE_SOURCE_DIR}/../core build)

add_executable(${PROJECT_NAME}.elf
  src/file_picker.cpp
)

set_target_properties(${PROJECT_NAME}.elf PROPERTIES CXX_STANDARD 17)

target_compile_definitions(gbemu_core PRIVATE
  DOCTEST_CONFIG_DISABLE
  )


target_link_libraries(${PROJECT_NAME}.elf
  gbemu_core
  stdc++fs
)

target_compile_options(${PROJECT_NAME}.elf PRIVATE
 -Wall
 -Wextra
 -Wshadow
 -Wpedantic
 -Wdouble-promotion
 -ggdb3
 )
