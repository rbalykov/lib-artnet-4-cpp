cmake_minimum_required(VERSION 3.31.2)
project(ArtNetController)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set a default build type if none is specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# Set compiler to clang if available, otherwise use default
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(STATUS "Using Clang compiler.")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Weverything -Wno-c++98-compat -Wno-gnu-anonymous-struct -Wno-padded -Wno-c++98-compat-pedantic")
else()
    message(STATUS "Using other compiler. (not clang)")
endif()

# Compiler flags based on build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Setting Debug Build Flags")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
else()
    message(STATUS "Setting Release Build Flags")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
endif()


# Subdirectory for the main artnet code
add_subdirectory(artnet)

# Example project
add_subdirectory(artnet/example)

# Custom target to build example, using out-of-source build
add_custom_target(build_artnet_example
    COMMAND ${CMAKE_COMMAND} -S . -B build
    COMMAND ${CMAKE_COMMAND} --build build
    COMMENT "Building Art-Net Example"
)

