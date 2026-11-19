cmake_minimum_required(VERSION 3.20)
project(seam_carving_cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Single-config generators default to an unoptimised build; don't.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SEAMCARVE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

add_library(seam_warnings INTERFACE)
if(MSVC)
  target_compile_options(seam_warnings INTERFACE /W4 /permissive- /utf-8)
  if(SEAMCARVE_WARNINGS_AS_ERRORS)
    target_compile_options(seam_warnings INTERFACE /WX)
  endif()
else()
  target_compile_options(seam_warnings INTERFACE -Wall -Wextra -Wpedantic)
  if(SEAMCARVE_WARNINGS_AS_ERRORS)
    target_compile_options(seam_warnings INTERFACE -Werror)
  endif()
endif()

# Core: pure CPU, no windowing, no graphics API.
add_library(seamcore STATIC
  src/core/image.cpp
  src/core/energy.cpp
  src/core/cost.cpp
)
target_include_directories(seamcore
  PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/extern"   # so headers read "stb/stb_image.h"
)
target_link_libraries(seamcore PRIVATE seam_warnings)

add_executable(seamcarve src/cli/main.cpp)
target_link_libraries(seamcarve PRIVATE seamcore seam_warnings)

enable_testing()
add_executable(test_cost tests/test_cost.cpp)
target_link_libraries(test_cost PRIVATE seamcore seam_warnings)
add_test(NAME cost COMMAND test_cost)

# Viewer (raylib) is not wired up yet. When it is: FetchContent raylib, add
# src/viewer/main.cpp, link seamcore raylib. Keep the dependency one-way.
