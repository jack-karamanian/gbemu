cmake_minimum_required(VERSION 3.15)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(cpu_experiments)

find_package(SDL2 CONFIG REQUIRED)
find_package(doctest CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(glad REQUIRED)

add_subdirectory(../core ./gbemu_core_build)

add_library(imgui OBJECT
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

target_include_directories(imgui PRIVATE src/imgui)
target_link_libraries(imgui PRIVATE
  SDL2::SDL2
  glad::glad
)

target_compile_definitions(imgui PRIVATE
  IMGUI_IMPL_OPENGL_LOADER_GLAD
)

set_target_properties(imgui PROPERTIES
  CXX_STANDARD 17
  INTERPROCEDURAL_OPTIMIZATION ${GBEMU_ENABLE_LTO}
)

set(SRCS
  src/main.cpp
  src/debugger/disassembly_view.h
  src/debugger/disassembly_view.cpp
  src/debugger/hardware_thread.h
  src/debugger/hardware_thread.cpp
  src/imgui_memory_editor.h
)

add_executable(cpu_experiments ${SRCS} $<TARGET_OBJECTS:imgui>)

set_target_properties(cpu_experiments PROPERTIES
  CXX_STANDARD 17
  INTERPROCEDURAL_OPTIMIZATION ${GBEMU_ENABLE_LTO}
)

find_library(capstone_lib capstone)

target_link_libraries(cpu_experiments PRIVATE
  SDL2::SDL2
  SDL2::SDL2main
  glad::glad
  gbemu_core
  fmt::fmt
  ${capstone_lib}
  gbemu_warnings
)

target_include_directories(cpu_experiments 
  PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/imgui>
)

target_compile_definitions(cpu_experiments PRIVATE
  span_FEATURE_MEMBER_AT=1
  span_CONFIG_CONTRACT_VIOLATION_THROWS
)

target_link_options(${PROJECT_NAME} PRIVATE )

target_compile_options(${PROJECT_NAME} PRIVATE
 )
