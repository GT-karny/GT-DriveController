cmake_minimum_required(VERSION 3.15)
project(GT-DriveController VERSION 1.0.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Dependencies (via vcpkg)
find_package(pybind11 CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(osi CONFIG REQUIRED)

# Includes
include_directories(include include/fmi2)

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
    protobuf::libprotobuf
    osi::open_simulation_interface
)

# Windows specific
if(WIN32)
    target_compile_definitions(GT-DriveController PRIVATE FMI2_FUNCTION_PREFIX=GT_DriveController_)
    # Copy resources/python to build directory for convenience if needed, 
    # but the FMU structure creation is manual or separate step usually.
endif()

# Installation / FMU Packaging helper (Optional)
# This assumes a manual zip step as per README, but we can stage it.
install(TARGETS GT-DriveController RUNTIME DESTINATION binaries/win64)
