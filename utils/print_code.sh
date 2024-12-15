#!/bin/bash

FILES=(
    "artnet/example/CMakeLists.txt"
    "artnet/example/ArtNetExample.cpp"
    "artnet/network_interface_linux.h"
    "artnet/network_interface_linux.cpp"
    "artnet/network_interface_bsd.h"
    "artnet/network_interface_bsd.cpp"
    "artnet/artnet_types.h"
    "artnet/CMakeLists.txt"
    "artnet/ArtNetController.h"
    "artnet/ArtNetController.cpp"
    "Makefile"
    "CMakeLists.txt"
)

for file in "${FILES[@]}"
do
    echo "Displaying: $file"
    nl -ba "$file"
    echo "End of $file"
    echo ""  # Print an empty line for better separation between files
done