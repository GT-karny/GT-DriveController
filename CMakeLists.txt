cmake_minimum_required(VERSION 3.15)
project(GT-DriveController VERSION 1.0.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Dependencies (via local submodule)
add_subdirectory(thirdparty/pybind11)
# OSI/Protobuf not needed in C++ (Python handles parsing)

# Includes
include_directories(include include/fmi2)

# Python Paths (Manual Override)
# Python Paths (Manual Override)
set(PYTHON_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/cpython")
set(PYTHON_INCLUDE_DIRS 
    "${PYTHON_SOURCE_DIR}/Include" 
    "${PYTHON_SOURCE_DIR}/PC"
)
include_directories(${PYTHON_INCLUDE_DIRS})

# Link against python312.lib AND python3.lib
set(PYTHON_LIBRARY 
    "${PYTHON_SOURCE_DIR}/PCbuild/amd64/python312.lib"
    "${PYTHON_SOURCE_DIR}/PCbuild/amd64/python3.lib"
)

# FMU Source Files
set(SOURCES
    src/main.cpp
    src/OSMPController.cpp
)

# Define FMU Shared Library
add_library(GT-DriveController SHARED ${SOURCES})

# Link Libraries
target_link_libraries(GT-DriveController PRIVATE
    pybind11::embed
)

# Windows specific
# Test Runner
add_executable(test_fmu tests/test_fmu.cpp)
target_include_directories(test_fmu PRIVATE include include/fmi2)
# We don't link GT-DriveController, we load it dynamically, but we need to ensure it's built.
add_dependencies(test_fmu GT-DriveController)

if(WIN32)
    # FMI2_FUNCTION_PREFIX should NOT be defined for DLLs as per fmi2Functions.h
    # target_compile_definitions(GT-DriveController PRIVATE FMI2_FUNCTION_PREFIX=GT_DriveController_)
    # Copy resources/python to build directory for convenience if needed, 
    # but the FMU structure creation is manual or separate step usually.
endif()

# Installation / FMU Packaging helper (Optional)
# This assumes a manual zip step as per README, but we can stage it.
install(TARGETS GT-DriveController RUNTIME DESTINATION binaries/win64)
