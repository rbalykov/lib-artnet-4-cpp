cmake_minimum_required(VERSION 3.31.2)
project(ArtNetController)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_EXTENSIONS OFF)

# set compiler to clang if available, otherwise use default
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(STATUS "Using Clang compiler.")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Weverything -Wno-c++98-compat -Wno-gnu-anonymous-struct -Wno-padded -Wno-c++98-compat-pedantic")
else()
  message(STATUS "Using other compiler. (not clang)")
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

# set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
