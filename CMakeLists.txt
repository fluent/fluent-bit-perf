cmake_minimum_required(VERSION 2.8)
project(fluent-bit-perf)

# Change output path for binaries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")

# Headers path
include_directories(include/)

# Tools
add_subdirectory(src)
