#include "../ArtNetController.h"

#include <chrono>
#include <iomanip> //  Include this header for std::setw
#include <iostream>
#include <random> // For generating random values
#include <thread>
#include <vector>

// void mydatacallback(uint16_t universe, const uint8_t *data, uint16_t length);
//
// void mydatacallback(uint16_t universe, const uint8_t *data, uint16_t length)
// {
//   std::cout << "received dmx data on universe: " << universe
//             << ", length: " << length << ", data: ";
//   for (int i = 0; i < length; ++i) {
//     std::cout << static_cast<int>(data[i]) << " ";
//   }
//   std::cout << std::endl;
// }

int main() {
  ArtNet::ArtNetController controller;

  // Configure the controller
  if (!controller.configure("0.0.0.0", ArtNet::ARTNET_PORT, 0, 0, 0,
                            "192.168.0.255")) {
    std::cerr << "artnet: configuration error" << std::endl;
    return 1;
  }

  if (!controller.start()) {
    std::cerr << "artnet: start error" << std::endl;
    return 1;
  }

  // Initialize DMX data
  std::vector<uint8_t> dmxData(512);

  // Random number generation setup
  std::random_device rd;
  std::mt19937 gen(rd()); // Mersenne Twister random number generator
  std::uniform_int_distribution<> dis(
      0, 255); // Generate numbers in range [0, 255]

  while (controller.isRunning()) {
    // Populate DMX data with random values for all channels
    for (size_t i = 0; i < 512; i++) {
      dmxData[i] = static_cast<uint8_t>(dis(gen)); // Assign random value
    }

    // Log DMX values in a formatted way
    std::cout << "DMX Values [showing first 32 channels]:" << std::endl;
    for (size_t i = 0; i < 32; i++) {
      std::cout << std::setw(3) << static_cast<int>(dmxData[i]);
      if ((i + 1) % 8 == 0) {
        std::cout << std::endl;
      } else {
        std::cout << " ";
      }
    }
    std::cout << "..." << std::endl;

    // Set the DMX data for universe 0
    controller.setDmxData(0, dmxData);

    // Send the DMX data
    if (!controller.sendDmx()) {
      std::cout << "artnet: send error" << std::endl;
    }

    // Maintain a ~30 FPS update rate
    std::this_thread::sleep_for(std::chrono::milliseconds(33));
  }

  return 0;
}
