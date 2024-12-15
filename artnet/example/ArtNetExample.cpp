#include "../ArtNetController.h"

#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

// Global signal handler for clean shutdown
static volatile bool running = true;
static void signalHandler(int) { running = false; }

int main() {
  // Setup signal handling for clean exit
  signal(SIGINT, signalHandler);  // Handle Ctrl+C
  signal(SIGTERM, signalHandler); // Handle termination request

  ArtNet::ArtNetController controller;

  // Configure the controller
  if (!controller.configure("0.0.0.0", ArtNet::ARTNET_PORT, 0, 0, 0,
                            "192.168.0.255")) {
    std::cerr << "artnet: configuration error" << std::endl;
    return 1;
  }

  // Random number generation setup
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 255);

  // Create our frame generator function
  auto frameGenerator = [&]() -> std::vector<uint8_t> {
    std::vector<uint8_t> dmxData(512);

    // Populate DMX data with random values
    for (size_t i = 0; i < dmxData.size(); i++) {
      dmxData[i] = static_cast<uint8_t>(dis(gen));
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

    return dmxData;
  };

  // Start the controller with our frame generator at 30 FPS
  if (!controller.start(frameGenerator, 30)) {
    std::cerr << "artnet: start error" << std::endl;
    return 1;
  }

  std::cout << "ArtNet controller running at 30 FPS. Press Ctrl+C to exit."
            << std::endl;

  // Main loop - just monitor statistics until shutdown requested
  while (running) {
    auto stats = controller.getStatistics(); // Now gets a Snapshot
    std::cout << "\rFrames: " << stats.totalFrames
              << " | Queue: " << stats.queueDepth
              << " | Dropped: " << stats.droppedFrames
              << " | Frame time: " << stats.lastFrameTime.count() << "µs"
              << std::flush;

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  std::cout << "\nShutting down..." << std::endl;
  controller.stop();

  return 0;
}
