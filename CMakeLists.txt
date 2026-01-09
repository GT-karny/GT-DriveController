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
# FMU Source Files
set(SOURCES
    src/main.cpp
    src/OSMPController.cpp
)

# Implementation Library (The logic that needs Python)
add_library(GT-DriveController_Core SHARED ${SOURCES})

# Core uses Python
target_link_libraries(GT-DriveController_Core PRIVATE
    pybind11::embed
)

# Windows specific for Core
if(WIN32)
    # Ensure exported functions are not mangled (though FMI standard defines extern C)
endif()

# Shim Library (The entry point that has NO Python dependency)
add_library(GT-DriveController SHARED src/shim.cpp)
target_include_directories(GT-DriveController PRIVATE include include/fmi2)

# Shim links only to system libs (default)

# Test Runner needs to link to Core or Shim? 
# The test runner usually calls fmi2Instantiate. 
# It can link to Shim.
add_executable(test_fmu tests/test_fmu.cpp)
target_include_directories(test_fmu PRIVATE include include/fmi2)
# Link test_fmu to Shim (not core, to verify loading)
# Actually, test_fmu source uses explicit LoadLibrary sometimes? 
# checking tests/test_fmu.cpp... it likely treats it as a lib.
# But for FMU testing, we often manually instantiate class.
# "test_fmu" links against the library.
# If test_fmu links against Shim, it will work.
# But test_fmu might access OSMPController class directly?
# Let's check test_fmu source later. For now, link dependencies.
add_dependencies(test_fmu GT-DriveController GT-DriveController_Core)

# Installation / Output
install(TARGETS GT-DriveController GT-DriveController_Core RUNTIME DESTINATION binaries/win64)
