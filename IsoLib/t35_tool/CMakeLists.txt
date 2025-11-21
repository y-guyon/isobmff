cmake_minimum_required(VERSION 3.16)

# Name & languages
project(t35_tool LANGUAGES C CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Includes: libisomediafile core
include_directories(
  ../libisomediafile/src
)

# Platform-specific include roots for libisomediafile
include_directories(
  # Linux
  $<$<PLATFORM_ID:Linux>:${CMAKE_CURRENT_LIST_DIR}/../libisomediafile/linux>
  # Windows
  $<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_LIST_DIR}/../libisomediafile/w32>
  # macOS
  $<$<PLATFORM_ID:Darwin>:${CMAKE_CURRENT_LIST_DIR}/../libisomediafile/macosx>
)

# common warnings
if (MSVC)
  add_compile_options(/W4)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()


# JSON dependency
include(FetchContent)
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(json)

# CLI11 (header-only)
FetchContent_Declare(
  cli11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11
  GIT_TAG        v2.5.0
)
FetchContent_MakeAvailable(cli11)

# ---- Executable ----
add_executable(
  t35_tool
  t35_tool.cpp
  SMPTE_ST2094_50.hpp
  SMPTE_ST2094_50.cpp 
)

# Link libraries
target_link_libraries(
  t35_tool

  PRIVATE
    libisomediafile
    nlohmann_json::nlohmann_json
    CLI11::CLI11
)
