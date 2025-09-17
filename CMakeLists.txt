cmake_minimum_required(VERSION 3.14)
project(pysfizz LANGUAGES CXX C)

# Find Python
find_package(Python COMPONENTS Interpreter Development REQUIRED)

# Add subdirectories for dependencies
add_subdirectory(external/nanobind)
add_subdirectory(external/sfizz)

# Configure sfizz for minimal build
set(SFIZZ_SHARED OFF CACHE BOOL "Disable shared library")

# Create Python extension
nanobind_add_module(pysfizz pysfizz/bindings.cpp)
target_link_libraries(pysfizz PRIVATE sfizz::static)

target_include_directories(pysfizz PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/external/sfizz/external/abseil-cpp
    ${CMAKE_SOURCE_DIR}/external/sfizz/external/simde/
    ${CMAKE_SOURCE_DIR}/external/sfizz/external/filesystem/include/
)
