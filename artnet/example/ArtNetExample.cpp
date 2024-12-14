#include "../ArtNetController.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

// Data callback function
void myDataCallback(uint16_t universe, const uint8_t *data, uint16_t length) {
  std::cout << "Received DMX data on universe: " << universe
            << ", length: " << length << ", data: ";
  for (int i = 0; i < length; ++i) {
    std::cout << std::setfill('0') << std::setw(2) << std::hex
              << static_cast<int>(data[i]) << " ";
  }
  std::cout << std::dec << std::endl;
}

int main() {
  ArtNet::ArtNetController controller;

  // Configure the controller
  if (!controller.configure("0.0.0.0", ArtNet::ARTNET_PORT, 0, 0, 0,
                            "255.255.255.255")) {
    std::cerr << "ArtNet: Configuration error" << std::endl;
    return 1;
  }

  if (!controller.start()) {
    std::cerr << "ArtNet: Start error" << std::endl;
    return 1;
  }

  // Set a callback
  controller.registerDataCallback(myDataCallback);

  // Send DMX data
  std::vector<uint8_t> dmxData(512);
  for (int i = 0; i < 512; i++) {
    dmxData[i] = (uint8_t)i;
  }

  controller.setDmxData(0, dmxData);

  while (controller.isRunning()) {

    if (!controller.sendDmx())
      std::cout << "ArtNet: send error" << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(33));
  }

  controller.stop();
  return 0;
}
