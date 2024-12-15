# lib-artnet-4-cpp

A modern C++ implementation of the Art-Net 4 protocol. This library provides a robust and efficient way to communicate using the Art-Net protocol, commonly used in lighting control and DMX over Ethernet applications.

<img width="1397" alt="image" src="https://github.com/user-attachments/assets/89a7acab-3a7d-4c11-bcc6-deb9de07a78c" />

## Features

- [ ] Full Art-Net 4 protocol support (WIP)
- Cross-platform compatibility (Linux and BSD/macOS)
- Modern C++ (C++17) implementation
- Thread-safe DMX data handling
- Support for multiple universes
- Configurable network parameters
- Simple and intuitive API

## Requirements

- C++17 compatible compiler
- CMake 3.31.2 or higher
- Network connectivity

## Building

The library uses CMake as its build system. Here's how to build it:

```bash
# Create a build directory
mkdir build
cd build

# Configure the project
cmake ..

# Build
make
```

You can also use the provided Makefile shortcuts:

```bash
# Debug build
make build_artnet_example_debug

# Release build
make build_artnet_example_release
```

## Usage Example

Here's a basic example of how to use the library to send DMX data:

```cpp
#include "../ArtNetController.h"
#include <vector>
#include <iostream>

int main() {
    ArtNet::ArtNetController controller;
    
    // Configure the controller
    // Parameters: bind address, port, net, subnet, universe, broadcast address
    if (!controller.configure("0.0.0.0", ArtNet::ARTNET_PORT, 0, 0, 0, "192.168.0.255")) {
        std::cerr << "Failed to configure Art-Net controller" << std::endl;
        return 1;
    }

    // Start the controller
    if (!controller.start()) {
        std::cerr << "Failed to start Art-Net controller" << std::endl;
        return 1;
    }

    // Prepare DMX data (512 channels)
    std::vector<uint8_t> dmxData(512);
    
    // Set some DMX values
    for (size_t i = 0; i < dmxData.size(); i++) {
        dmxData[i] = static_cast<uint8_t>(i % 255);
    }

    // Send DMX data
    controller.setDmxData(0, dmxData);
    controller.sendDmx();

    return 0;
}
```

## API Reference

### ArtNetController Class

The main class for Art-Net operations.

#### Constructor
```cpp
ArtNetController();
```

#### Configuration
```cpp
bool configure(const std::string &bindAddress, 
              int port,
              uint8_t net,
              uint8_t subnet,
              uint8_t universe,
              const std::string &broadcastAddress = "255.255.255.255");
```

#### Network Control
```cpp
bool start();
void stop();
bool isRunning() const;
```

#### DMX Operations
```cpp
bool setDmxData(uint16_t universe, const std::vector<uint8_t> &data);
bool setDmxData(uint16_t universe, const uint8_t *data, size_t length);
std::vector<uint8_t> getDmxData(uint16_t universe);
bool sendDmx();
```

### Network Interface Classes

The library provides platform-specific network implementations:
- `NetworkInterfaceLinux` for Linux systems
- `NetworkInterfaceBSD` for BSD-based systems (including macOS)

## Thread Safety

The library implements thread-safe operations for DMX data handling using mutex locks. Multiple threads can safely call methods on the same `ArtNetController` instance.

## Art-Net Protocol Support

The library supports the following Art-Net 4 features:
- DMX512 transmission
- Multiple universe support
- Network configuration
- Sequence numbering
- Art-Net packet formatting according to specification

## Known Limitations

- Currently focuses on DMX transmission (ArtDmx packets)
- Does not implement RDM functionality
- Limited to UDP broadcast/unicast (no TCP support)

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request. For major changes, please open an issue first to discuss what you would like to change.

## License

This project is licensed under the GPL-3.0 License - see the LICENSE file for details.

## Author

Gaston Morixe (gaston@gastonmorixe.com)

Copyright © 2024

## Acknowledgments

- Based on the official [Art-Net 4 Protocol Specification](https://artisticlicence.com/WebSiteMaster/User%20Guides/art-net.pdf)
- Thanks to Artistic Licence for creating and maintaining the Art-Net protocol
