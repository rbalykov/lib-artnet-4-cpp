# Executable name
EXECUTABLE = artnet_example

# Custom target to build example (using cmake)
build_artnet_example:
	cmake -S . -B build && make -C build

run_artnet_example: build_artnet_example
	./build/artnet/example/artnet_example

.PHONY: build_artnet_example run_artnet_example

# Compiler and flags
# CXX = g++
# CXXFLAGS = -std=c++17 -pthread -Wall -Wextra

# # Executable name
# EXECUTABLE = artnet_example
#
# # Custom target to build example (using cmake)
# build_artnet_example:
# 	cmake -S . -B build && make -C build
#
# .PHONY: build_artnet_example
