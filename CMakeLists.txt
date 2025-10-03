cmake_minimum_required(VERSION 3.15)
project(pysfizz LANGUAGES CXX C)

# Find Python
find_package(Python COMPONENTS Interpreter Development REQUIRED)

# Add subdirectories for dependencies
add_subdirectory(external/nanobind)
add_subdirectory(external/sfizz)

# Configure sfizz for minimal build
set(SFIZZ_SHARED OFF CACHE BOOL "Disable shared library")

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