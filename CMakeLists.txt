cmake_minimum_required(VERSION 3.15)
project(pysfizz LANGUAGES CXX C)

# Find Python
find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)

# Add nanobind
add_subdirectory(external/nanobind)

# ============================================================
# DYNAMIC PATCH: Fix sfizz's /MT to use /MD on Windows
# ============================================================
if(MSVC)
  set(SFIZZ_CONFIG_ORIGINAL "${CMAKE_CURRENT_SOURCE_DIR}/external/sfizz/cmake/SfizzConfig.cmake")
  set(SFIZZ_CONFIG_PATCHED "${CMAKE_CURRENT_BINARY_DIR}/sfizz_patched/SfizzConfig.cmake")
  
  file(READ "${SFIZZ_CONFIG_ORIGINAL}" SFIZZ_CONFIG_CONTENT)
  string(REPLACE 
    "set(CMAKE_MSVC_RUNTIME_LIBRARY \"MultiThreaded$<$<CONFIG:Debug>:Debug>\")"
    "set(CMAKE_MSVC_RUNTIME_LIBRARY \"MultiThreaded$<$<CONFIG:Debug>:Debug>DLL\")"
    SFIZZ_CONFIG_CONTENT
    "${SFIZZ_CONFIG_CONTENT}"
  )
  file(WRITE "${SFIZZ_CONFIG_PATCHED}" "${SFIZZ_CONFIG_CONTENT}")
  
  set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_BINARY_DIR}/sfizz_patched;${CMAKE_MODULE_PATH}")
endif()
# ============================================================

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

install(TARGETS _sfizz LIBRARY DESTINATION pysfizz)