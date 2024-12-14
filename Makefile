# Executable name
EXECUTABLE = artnet_example

# Custom target to build debug example (using cmake)
build_artnet_example_debug:
	cmake -DCMAKE_BUILD_TYPE=Debug -S . -B build && make -C build

# Custom target to build release example (using cmake)
build_artnet_example_release:
	cmake -DCMAKE_BUILD_TYPE=Release -S . -B build && make -C build

# run_artnet_example: build_artnet_example_debug
# 	./build/artnet/example/artnet_example

.PHONY: build_artnet_example_debug build_artnet_example_release 
	# run_artnet_example
