cmake_minimum_required(VERSION 3.14)
project(pysfizz LANGUAGES CXX C)

# Find Python
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

# Set Python paths for nanobind
set(Python_EXECUTABLE ${Python3_EXECUTABLE})
set(Python_INCLUDE_DIRS ${Python3_INCLUDE_DIRS})
set(Python_LIBRARIES ${Python3_LIBRARIES})

# Add subdirectories for dependencies
add_subdirectory(external/nanobind)
add_subdirectory(external/sfizz)

# Configure sfizz for minimal build
set(SFIZZ_SHARED OFF CACHE BOOL "Disable shared library")

# Create Python extension
nanobind_add_module(pysfizz pysfizz/bindings.cpp)
target_link_libraries(pysfizz PRIVATE sfizz::static)
