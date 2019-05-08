cmake_minimum_required(VERSION 3.8)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(gbemu)

add_subdirectory(${CMAKE_SOURCE_DIR}/../core build)

add_executable(${PROJECT_NAME} src/main.cpp)

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

find_library(BOOST_PROGRAM_OPTIONS_LIB boost_program_options)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_core ${BOOST_PROGRAM_OPTIONS_LIB})

target_compile_options(${PROJECT_NAME} PRIVATE
 -Wall
 -Wextra
 -Wshadow
 -Wpedantic
 -Wdouble-promotion
 -ggdb3
 )
