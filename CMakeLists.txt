cmake_minimum_required(VERSION 3.15)

# Enable CMAKE_MSVC_RUNTIME_LIBRARY
if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()

project(pysfizz LANGUAGES CXX C)

# Use /MD runtime on Windows for Python compatibility
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "MSVC runtime library")
endif()

# Find Python
find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)

# Add nanobind
add_subdirectory(external/nanobind)

# Configure and add sfizz
set(WAVPACK_ENABLE_ASM OFF CACHE BOOL "Disable WavPack assembly")
set(SFIZZ_JACK OFF CACHE BOOL "Disable JACK support")
set(SFIZZ_SHARED OFF CACHE BOOL "Disable shared library")
add_subdirectory(external/sfizz)

# Create Python extension
nanobind_add_module(_sfizz pysfizz/bindings.cpp)
target_link_libraries(_sfizz PRIVATE sfizz::static)

target_include_directories(_sfizz PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/external/sfizz/external/abseil-cpp
    ${CMAKE_SOURCE_DIR}/external/sfizz/external/simde/
    ${CMAKE_SOURCE_DIR}/external/sfizz/external/filesystem/include/
)

add_custom_command(TARGET _sfizz POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy 
        $<TARGET_FILE:_sfizz>
        ${CMAKE_SOURCE_DIR}/pysfizz/
    COMMENT "Copying _sfizz module to pysfizz/ directory"
)