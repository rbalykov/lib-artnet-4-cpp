#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "artnet_types.h"

namespace ArtNet {

// forward declaration
class NetworkInterface;

class ArtNetController {
public:
  // Data Handling
  using DataCallback = std::function<void(
      uint16_t universe, const uint8_t *data, uint16_t length)>;

  ArtNetController();
  ~ArtNetController();

  // Configuration
  bool configure(const std::string &bindAddress, int port, uint8_t net,
                 uint8_t subnet, uint8_t universe,
                 const std::string &broadcastAddress = "255.255.255.255");

  // Networking
  bool start();
  void stop();
  bool isRunning() const;

  // Data Management
  bool setDmxData(uint16_t universe, const std::vector<uint8_t> &data);
  bool setDmxData(uint16_t universe, const uint8_t *data, size_t length);
  std::vector<uint8_t> getDmxData(uint16_t universe); // Added getDmxData

  // Sending
  bool sendDmx();
  bool sendPoll();

  // Receiving
  void registerDataCallback(DataCallback callback);

private:
  // Network Related
  std::unique_ptr<NetworkInterface> m_networkInterface;

  // --- Art-Net Parameters ---
  std::string m_bindAddress;
  int m_port;
  std::string m_broadcastAddress;

  uint8_t m_net;
  uint8_t m_subnet;
  uint8_t m_universe;

  // --- Internal State ---
  bool m_isRunning = false;
  bool m_isConfigured = false;
  bool m_enableReceiving = false; // Default: disabled
  std::thread m_receiveThread;
  std::mutex m_dataMutex;
  std::vector<uint8_t> m_dmxData; // This will be removed in future steps
  uint16_t m_seqNumber;
  DataCallback m_dataCallback;

  // --- Core Logic ---
  bool prepareArtDmxPacket(uint16_t universe, const uint8_t *data,
                           size_t length, std::vector<uint8_t> &packet);
  bool prepareArtPollPacket(std::vector<uint8_t> &packet);
  bool sendPacket(const std::vector<uint8_t> &packet);

  void receivePackets();
  void handleArtPacket(const uint8_t *buffer, int size);
  void handleArtDmx(const uint8_t *buffer, int size);
  void handleArtPoll(const uint8_t *buffer, int size);
  void handleArtPollReply(const uint8_t *buffer, int size);
};

// Abstract class for network interface ( platform agnostic )
class NetworkInterface {
public:
  static constexpr size_t MAX_PACKET_SIZE = 2048;

public:
  virtual ~NetworkInterface() = default;
  virtual bool createSocket(const std::string &bindAddress, int port) = 0;
  virtual bool bindSocket() = 0;
  virtual bool sendPacket(const std::vector<uint8_t> &packet,
                          const std::string &address, int port) = 0;
  virtual int receivePacket(std::vector<uint8_t> &buffer) = 0;
  virtual void closeSocket() = 0;
};

} // namespace ArtNet
