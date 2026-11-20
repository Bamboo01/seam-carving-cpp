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
  src/core/seam_ops.cpp
  src/core/seam_search.cpp
  src/core/carver.cpp
)
target_include_directories(seamcore
  PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/extern"   # so headers read "stb/stb_image.h"
)
target_link_libraries(seamcore PRIVATE seam_warnings)

add_executable(seamcarve src/cli/main.cpp)
target_link_libraries(seamcarve PRIVATE seamcore seam_warnings)

# Tests: one executable per module, run with ctest.
enable_testing()

add_executable(test_cost tests/test_cost.cpp)
target_link_libraries(test_cost PRIVATE seamcore seam_warnings)
add_test(NAME cost COMMAND test_cost)

add_executable(test_seam_ops tests/test_seam_ops.cpp)
target_link_libraries(test_seam_ops PRIVATE seamcore seam_warnings)
add_test(NAME seam_ops COMMAND test_seam_ops)

add_executable(test_seam_search tests/test_seam_search.cpp)
target_link_libraries(test_seam_search PRIVATE seamcore seam_warnings)
add_test(NAME seam_search COMMAND test_seam_search)

# ---------------------------------------------------------------------------
# Viewer: raylib only, one-way dependency on seamcore.
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

add_executable(seamcarve_viewer
  src/viewer/main.cpp
  src/viewer/app.cpp
  src/viewer/ui.cpp
)
target_link_libraries(seamcarve_viewer PRIVATE seamcore raylib seam_warnings)
