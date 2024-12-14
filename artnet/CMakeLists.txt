# Source files for our artnet lib
set(ARTNET_SRC
    ArtNetController.cpp
    network_interface_bsd.cpp
    network_interface_linux.cpp
)

# Header files for our artnet lib
set(ARTNET_HDR
   ArtNetController.h
   artnet_types.h
   network_interface_bsd.h
   network_interface_linux.h
)

# Define an artnet library
add_library(artnet
    ${ARTNET_SRC}
    ${ARTNET_HDR}
)

# Set include for our artnet lib
target_include_directories(artnet PUBLIC .)
