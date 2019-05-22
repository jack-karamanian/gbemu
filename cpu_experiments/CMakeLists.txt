cmake_minimum_required(VERSION 3.8)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(cpu_experiments)


find_package(SDL2 CONFIG REQUIRED)
find_package(doctest CONFIG REQUIRED)
find_package(LLVM CONFIG REQUIRED)

set(SRCS
  src/main.cpp
  src/assembler.h
  src/assembler.cpp
  src/imgui/examples/imgui_impl_sdl.h
  src/imgui/examples/imgui_impl_sdl.cpp
  src/imgui/examples/imgui_impl_opengl3.cpp
  src/imgui/misc/cpp/imgui_stdlib.cpp
  src/imgui/imgui_demo.cpp
  src/imgui/imgui.cpp
  src/imgui/imgui_draw.cpp
  src/imgui/imgui_widgets.cpp
  src/imgui/imgui.h
)

add_executable(cpu_experiments ${SRCS})
set_target_properties(cpu_experiments PROPERTIES CXX_STANDARD 17)

target_link_libraries(cpu_experiments PRIVATE SDL2::SDL2-static GLEW GL LLVM)

target_include_directories(cpu_experiments 
  PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/imgui>
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/../core/src>
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/../core/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../external/span-lite/include>

)

target_compile_definitions(cpu_experiments PRIVATE
  IMGUI_IMPL_OPENGL_LOADER_CUSTOM=<GL/glew.h> 
  span_FEATURE_MEMBER_AT=1
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
