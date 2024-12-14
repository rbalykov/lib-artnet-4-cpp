# source file for the example
set(ARTNET_EXAMPLE_SRC
    ArtNetExample.cpp
)

add_executable(artnet_example ${ARTNET_EXAMPLE_SRC})

# linking with our artnet lib
target_link_libraries(artnet_example artnet)
