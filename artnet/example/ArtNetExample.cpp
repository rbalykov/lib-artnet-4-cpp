#include "../ArtNetController.h"
#include "../logging.h"
#include "../utils.h"

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <string_view>
#include <thread>
#include <vector>

void myDataCallback(uint16_t universe, const uint8_t *data, uint16_t length);
void myDataCallback(uint16_t universe, const uint8_t *data, uint16_t length) {
  std::cout << "myDataCallback: Received DMX data on universe: " << universe << ", length: " << length << ", data: ";
  for (int i = 0; i < length; ++i) {
    std::cout << static_cast<int>(data[i]) << " ";
  }
  std::cout << std::endl;
}

static volatile bool running = true;
static void signalHandler(int) { running = false; }

struct Config {
  std::string bindAddress = "0.0.0.0";
  int port = ArtNet::ARTNET_PORT;
  uint8_t net = 0;
  uint8_t subnet = 0;
  uint8_t universe = 0;
  std::string broadcastAddress = "192.168.0.255";
  ArtNet::LogLevel logLevel = ArtNet::LogLevel::ERROR;
};

void printUsage(const char *programName) {
  std::cout << "Usage: " << programName << " [options]\n"
            << "Options:\n"
            << "  --bind=ADDRESS       Binding IP address (default: 0.0.0.0)\n"
            << "  --port=PORT          Art-Net port (default: 6454)\n"
            << "  --net=N              Art-Net net (0-127, default: 0)\n"
            << "  --subnet=N           Art-Net subnet (0-15, default: 0)\n"
            << "  --universe=N         Art-Net universe (0-15, default: 0)\n"
            << "  --broadcast=ADDRESS  Broadcast IP address (default: 192.168.0.255)\n"
            << "  --verbose[=LEVEL]    Set verbosity level (1=error, 2=info, 3=debug)\n"
            << "  --help               Show this help message\n\n"
            << "Examples:\n"
            << "  " << programName << " --bind=192.168.1.100 --broadcast=192.168.1.255\n"
            << "  " << programName << " --net=1 --subnet=2 --universe=3 --verbose=2\n";
}

bool parseArgs(int argc, char *argv[], Config &config) {
  for (int i = 1; i < argc; ++i) {
    std::string_view arg(argv[i]);

    if (arg == "--help") {
      printUsage(argv[0]);
      return false;
    }

    auto getValue = [](std::string_view arg) -> std::string_view {
      size_t pos = arg.find('=');
      if (pos != std::string_view::npos) {
        return arg.substr(pos + 1);
      }
      return "";
    };

    if (arg.compare(0, 7, "--bind=") == 0) {
      config.bindAddress = std::string(getValue(arg));
    } else if (arg.compare(0, 7, "--port=") == 0) {
      config.port = std::atoi(getValue(arg).data());
    } else if (arg.compare(0, 6, "--net=") == 0) {
      int value = std::atoi(getValue(arg).data());
      if (value < 0 || value > 127) {
        std::cerr << "Error: Net must be between 0 and 127\n";
        return false;
      }
      config.net = static_cast<uint8_t>(value);
    } else if (arg.compare(0, 9, "--subnet=") == 0) {
      int value = std::atoi(getValue(arg).data());
      if (value < 0 || value > 15) {
        std::cerr << "Error: Subnet must be between 0 and 15\n";
        return false;
      }
      config.subnet = static_cast<uint8_t>(value);
    } else if (arg.compare(0, 11, "--universe=") == 0) {
      int value = std::atoi(getValue(arg).data());
      if (value < 0 || value > 15) {
        std::cerr << "Error: Universe must be between 0 and 15\n";
        return false;
      }
      config.universe = static_cast<uint8_t>(value);
    } else if (arg.compare(0, 12, "--broadcast=") == 0) {
      config.broadcastAddress = std::string(getValue(arg));
    } else if (arg.compare(0, 9, "--verbose") == 0) {
      int level = 2; // Default to INFO if no level specified
      size_t equals_pos = arg.find('=');
      if (equals_pos != std::string_view::npos) {
        level = std::atoi(arg.substr(equals_pos + 1).data());
      }
      config.logLevel = static_cast<ArtNet::LogLevel>(level);
    }
  }
  return true;
}

int main(int argc, char *argv[]) {
  Config config;

  if (!parseArgs(argc, argv, config)) {
    return 1;
  }

  // Set logging level
  ArtNet::Logger::setLevel(config.logLevel);

  // Try to set process priority
  if (!ArtNet::utils::setThreadPriority(ArtNet::utils::ThreadPriority::HIGH)) {
    ArtNet::Logger::info("Failed to set process priority. Try running with sudo.");
  }

  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);

  ArtNet::Logger::info("Starting Art-Net controller with configuration:");
  ArtNet::Logger::info("  Bind Address: ", config.bindAddress);
  ArtNet::Logger::info("  Port: ", config.port);
  ArtNet::Logger::info("  Net: ", static_cast<int>(config.net));
  ArtNet::Logger::info("  Subnet: ", static_cast<int>(config.subnet));
  ArtNet::Logger::info("  Universe: ", static_cast<int>(config.universe));
  ArtNet::Logger::info("  Broadcast: ", config.broadcastAddress);

  ArtNet::ArtNetController controller;

  // Configure the controller with command line parameters
  if (!controller.configure(config.bindAddress, config.port, config.net, config.subnet, config.universe, config.broadcastAddress)) {
    ArtNet::Logger::error("Configuration error");
    return 1;
  }

  // Random number generation setup
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 255);

  auto frameGenerator = [&]() -> std::vector<uint8_t> {
    std::vector<uint8_t> dmxData(512);

    for (size_t i = 0; i < dmxData.size(); i++) {
      dmxData[i] = static_cast<uint8_t>(dis(gen));
    }

    // Only show DMX values in debug mode
    ArtNet::Logger::debug("DMX Values [showing first 32 channels]:");
    if (ArtNet::Logger::getLevel() >= ArtNet::LogLevel::DEBUG) {
      for (size_t i = 0; i < 32; i++) {
        std::cout << std::setw(3) << static_cast<int>(dmxData[i]);
        if ((i + 1) % 8 == 0) {
          std::cout << std::endl; // TODO: Cout not! Use logger
        } else {
          std::cout << " ";
        }
      }
      std::cout << "..." << std::endl;
    }

    return dmxData;
  };

  // Set ArtNet callback
  controller.registerDataCallback(myDataCallback);

  // Start ArtNet controller
  if (!controller.start(frameGenerator, ArtNet::ARTNET_FPS)) {
    ArtNet::Logger::error("Start error");
    return 1;
  }

  // TODO: Do not user logger in Example. Logger should be inside the lib?
  ArtNet::Logger::info("Controller running at ", ArtNet::ARTNET_FPS, " FPS. Press Ctrl+C to exit.");

  while (running) {
    auto stats = controller.getStatistics();
    if (ArtNet::Logger::getLevel() >= ArtNet::LogLevel::INFO) {
      std::cout << "\rFrames: " << stats.totalFrames << " | Queue: " << stats.queueDepth << " | Dropped: " << stats.droppedFrames
                << " | Frame time: " << stats.lastFrameTime.count() << "µs" << std::endl
                << std::flush;
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  ArtNet::Logger::info("\nShutting down...");
  controller.stop();

  return 0;
}
