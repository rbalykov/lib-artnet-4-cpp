#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "NetworkInterface.h"
#include "artnet_types.h"

// forward declaration
// class NetworkInterface;

namespace ArtNet {

class ArtNetController {
public:
  // Data Handling
  using DataCallback = std::function<void(uint16_t universe, const uint8_t *data, uint16_t length)>;
  using FrameGenerator = std::function<std::vector<uint8_t>()>;

  // Statistics structure for monitoring
  struct Statistics {
    std::atomic<uint64_t> totalFrames{0};
    std::atomic<uint64_t> droppedFrames{0};
    std::atomic<size_t> queueDepth{0};
    std::chrono::microseconds lastFrameTime{0};

    struct Snapshot {
      uint64_t totalFrames;
      uint64_t droppedFrames;
      size_t queueDepth;
      std::chrono::microseconds lastFrameTime;
    };

    Snapshot getSnapshot() const { return Snapshot{totalFrames.load(), droppedFrames.load(), queueDepth.load(), lastFrameTime}; }
  };

  ArtNetController();
  ~ArtNetController();

  // Configuration
  bool configure(const std::string &bindAddress, int port, uint8_t net, uint8_t subnet, uint8_t universe,
                 const std::string &broadcastAddress = "255.255.255.255");

  // Networking
  bool start();
  bool start(FrameGenerator generator, int fps = 30);
  void stop();
  bool isRunning() const;

  // Data Management
  bool setDmxData(uint16_t universe, const std::vector<uint8_t> &data);
  bool setDmxData(uint16_t universe, const uint8_t *data, size_t length);
  std::vector<uint8_t> getDmxData(uint16_t universe);

  // Sending
  bool sendDmx();
  bool sendPoll();
  void sendPollReply(const uint8_t *buffer, int size);

  // Receiving
  void registerDataCallback(DataCallback callback);

  // Statistics
  // const Statistics &getStatistics() const { return m_stats; }
  Statistics::Snapshot getStatistics() const { return m_stats.getSnapshot(); }

private:
  void startFrameProcessor();

  // Network Related
  std::unique_ptr<NetworkInterface> m_networkInterface;

  // Art-Net Parameters
  std::string m_bindAddress;
  int m_port;
  std::string m_broadcastAddress;

  uint8_t m_net;
  uint8_t m_subnet;
  uint8_t m_universe;

  // Internal State
  static constexpr size_t MAX_QUEUE_SIZE = 4;
  bool m_isRunning = false;
  bool m_isConfigured = false;
  bool m_enableReceiving = false;
  std::thread m_receiveThread;
  std::mutex m_dataMutex;
  std::vector<uint8_t> m_dmxData;
  uint16_t m_seqNumber;
  DataCallback m_dataCallback;

  // Frame Processing
  std::queue<std::vector<uint8_t>> m_frameQueue;
  std::mutex m_queueMutex;
  std::thread m_processorThread;
  std::chrono::microseconds m_frameInterval;
  FrameGenerator m_frameGenerator;
  Statistics m_stats;

  // Core Logic
  bool prepareArtDmxPacket(uint16_t universe, const uint8_t *data, size_t length, std::vector<uint8_t> &packet);
  bool prepareArtPollPacket(std::vector<uint8_t> &packet);
  bool sendPacket(const std::vector<uint8_t> &packet, const std::string &address = "", int port = 0);

  void receivePackets();
  void handleArtPacket(const uint8_t *buffer, int size);
  void handleArtDmx(const uint8_t *buffer, int size);
  void handleArtPoll(const uint8_t *buffer, int size);
  void handleArtPollReply(const uint8_t *buffer, int size);

  // Node Discovery
  struct NodeInfo {
    std::array<uint8_t, 4> ip;
    uint16_t port;
    uint16_t oem;
    uint8_t netSwitch;
    uint8_t subSwitch;
    std::string shortName;
    std::string longName;
    std::vector<uint16_t> subscribedUniverses; // List of subscribed universes
    // Add other useful information from the ArtPollReply
  };

  std::map<std::string, NodeInfo> m_discoveredNodes;
  std::mutex m_nodesMutex;
};

} // namespace ArtNet
