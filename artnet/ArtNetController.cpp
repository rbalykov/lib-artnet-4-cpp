#include "ArtNetController.h"
#include "artnet_types.h"
#include "logging.h"
#include "utils.h"

#include <chrono>
#include <cstring>
#include <iomanip>
#include <random>
#include <sys/_endian.h>
#include <thread>

#ifdef __APPLE__
#include "network_interface_bsd.h"
#else
#include "network_interface_linux.h"
#endif

namespace ArtNet {

ArtNetController::ArtNetController()
    : m_port(ARTNET_PORT), m_net(0), m_subnet(0), m_universe(0), m_isRunning(false), m_seqNumber(0), m_dataCallback(nullptr),
      m_isConfigured(false), m_frameInterval(std::chrono::microseconds(1000000 / ARTNET_FPS)) {}

ArtNetController::~ArtNetController() { stop(); }

bool ArtNetController::configure(const std::string &bindAddress, int port, uint8_t net, uint8_t subnet, uint8_t universe,
                                 const std::string &broadcastAddress) {
  if (isRunning()) {
    Logger::error("Cannot configure while running");
    return false;
  }

  Logger::debug("Configuring controller: ", "bind=", bindAddress, " port=", port, " net=", static_cast<int>(net),
                " subnet=", static_cast<int>(subnet), " universe=", static_cast<int>(universe), " broadcast=", broadcastAddress);

  // Store configuration
  m_bindAddress = bindAddress;
  m_port = port;
  m_net = net;
  m_subnet = subnet;
  m_universe = universe;
  m_broadcastAddress = broadcastAddress.empty() ? "255.255.255.255" : broadcastAddress;
  m_isConfigured = true;

  Logger::info("Controller configured successfully");

  return true;
}

void ArtNetController::setEnableSendingDMX(bool enable) { m_enableSendingDMX = enable; }

bool ArtNetController::start() {
  if (!m_isConfigured) {
    Logger::error("Controller not configured, call configure() first");
    return false;
  }
  if (m_isRunning) {
    Logger::error("Already running");
    return false;
  }

#ifdef __APPLE__
  m_networkInterface = std::make_unique<NetworkInterfaceBSD>();
#else
  m_networkInterface = std::make_unique<NetworkInterfaceLinux>();
#endif

  if (!m_networkInterface->createSocket(m_bindAddress, m_port)) {
    return false;
  }

  if (!m_networkInterface->bindSocket()) {
    return false;
  }

  m_isRunning = true;

  m_receiveThread = std::thread(&ArtNetController::receivePackets, this);

  return true;
}

// New start method with frame generator
bool ArtNetController::start(FrameGenerator generator, int fps) {
  if (!start())
    return false;

  m_frameGenerator = std::move(generator);
  m_frameInterval = std::chrono::microseconds(1000000 / fps);
  startFrameProcessor();
  return true;
}

void ArtNetController::startFrameProcessor() {
  m_processorThread = std::thread([this]() {
    // Set high priority for this thread
    if (!utils::setThreadPriority(utils::ThreadPriority::HIGH)) {
      Logger::info("Failed to set high priority for frame processor thread. Try running with sudo or setting capability.");
    }

    auto nextFrame = std::chrono::steady_clock::now();

    while (m_isRunning) {
      auto frameStart = std::chrono::steady_clock::now();

      // Generate new frame
      if (m_frameGenerator) {
        try {
          auto dmxData = m_frameGenerator();

          // Lock only for queue operations
          {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (m_frameQueue.size() >= MAX_QUEUE_SIZE) {
              m_stats.droppedFrames++;
              m_frameQueue.pop();
            }
            m_frameQueue.push(std::move(dmxData));
            m_stats.queueDepth = m_frameQueue.size();
          }
        } catch (const std::exception &e) {
          Logger::error("Frame generator error: ", e.what());
        }
      }

      // Process queue
      std::vector<uint8_t> frame;
      {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (!m_frameQueue.empty()) {
          frame = std::move(m_frameQueue.front());
          m_frameQueue.pop();
          m_stats.queueDepth = m_frameQueue.size();
        }
      }

      // Send frame if available
      if (!frame.empty()) {
        setDmxData(m_universe, frame);
        if (sendDmx()) {
          m_stats.totalFrames++;
        }
      }

      // Calculate timing for next frame
      auto frameEnd = std::chrono::steady_clock::now();
      m_stats.lastFrameTime = std::chrono::duration_cast<std::chrono::microseconds>(frameEnd - frameStart);

      // Sleep until next frame
      nextFrame += m_frameInterval;
      if (frameEnd < nextFrame) {
        std::this_thread::sleep_until(nextFrame);
      }
    }
  });

  // Optionally set CPU affinity to bind the thread to a specific core
  // #ifdef __linux__
  //   cpu_set_t cpuset;
  //   CPU_ZERO(&cpuset);
  //   CPU_SET(0, &cpuset); // Bind to first CPU core
  //   pthread_setaffinity_np(m_processorThread.native_handle(), sizeof(cpu_set_t), &cpuset);
  // #endif
}

void ArtNetController::stop() {
  m_isRunning = false;

  // Stop frame processor thread
  if (m_processorThread.joinable()) {
    m_processorThread.join();
  }

  // Gracefully shut down the socket for receiving data
  if (m_networkInterface) {
    if (m_networkInterface->getSocket() != -1) {
      shutdown(m_networkInterface->getSocket(), SHUT_RD); // Shutdown receive operations
    }
  }

  // Stop receive thread
  if (m_receiveThread.joinable()) {
    m_receiveThread.join();
  }

  if (m_networkInterface) {
    m_networkInterface->closeSocket();
  }
}

bool ArtNetController::isRunning() const { return m_isRunning; }

void ArtNetController::logDmxData(const std::vector<uint8_t> &dmxData) {
  if (ArtNet::Logger::getLevel() < ArtNet::LogLevel::DEBUG) {
    return;
  }

  ArtNet::Logger::debug("DMX Values [showing first 32 channels]:");
  std::string line;
  for (size_t i = 0; i < 32; i++) {
    line += std::to_string(static_cast<int>(dmxData[i]));
    line += " ";
    if ((i + 1) % 8 == 0) {
      ArtNet::Logger::debug(line);
      line.clear();
    }
  }
  if (!line.empty()) {
    ArtNet::Logger::debug(line);
  }
  ArtNet::Logger::debug("...");
}

bool ArtNetController::setDmxData(uint16_t universe, const std::vector<uint8_t> &data) {
  if (data.size() > ARTNET_MAX_DMX_SIZE) {
    Logger::error("DMX data exceeds max size");
    return false;
  }
  if (universe != m_universe) {
    return false;
  }

  logDmxData(data);

  std::lock_guard<std::mutex> lock(m_dataMutex);
  m_dmxData = data;
  return true;
}

bool ArtNetController::setDmxData(uint16_t universe, const uint8_t *data, size_t length) {
  if (length > ARTNET_MAX_DMX_SIZE) {
    std::cerr << "ArtNet: DMX data exceeds max size" << std::endl;
    return false;
  }
  if (universe != m_universe) {
    return false;
  }

  std::lock_guard<std::mutex> lock(m_dataMutex);
  m_dmxData.assign(data, data + length);
  return true;
}

std::vector<uint8_t> ArtNetController::getDmxData(uint16_t universe) {
  std::lock_guard<std::mutex> lock(m_dataMutex);
  if (universe == m_universe)
    return m_dmxData;
  else
    return std::vector<uint8_t>();
}

bool ArtNetController::sendDmx() {
  if (!m_enableSendingDMX)
    return true; // Do nothing if sending is disabled

  std::vector<uint8_t> packet;

  {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    if (m_dmxData.empty())
      return false;

    if (!prepareArtDmxPacket(m_universe, m_dmxData.data(), m_dmxData.size(), packet))
      return false;
  }

  return sendPacket(packet);
}

bool ArtNetController::sendPoll() {
  std::vector<uint8_t> packet;

  if (!prepareArtPollPacket(packet)) {
    Logger::info("sendPoll: prepareArtDmxPacket false");
    return false;
  }

  return sendPacket(packet);
}

void ArtNetController::sendPollReply(const uint8_t *buffer, sockaddr_in senderAddr) {
  // Use senderAddr to reply in unicast
  std::string destIP = utils::formatIP(reinterpret_cast<const uint8_t *>(&senderAddr.sin_addr.s_addr), 4);
  int destPort = ntohs(senderAddr.sin_port);

  Logger::debug("--> sendPollReply start...", destIP, " Port: ", destPort);

  // Create the ArtPollReply packet
  ArtPollReplyPacket replyPacket;

  // Populate the ArtPollReply packet
  replyPacket.header = ArtHeader(OpCode::OpPollReply);

  // static_assert(sizeof(replyPacket.header) == 12, "ArtHeader must be exactly 12 bytes");

  // 1. IP Address (Correct parsing and assignment)
  replyPacket.ip[0] = 252;
  replyPacket.ip[1] = 253;
  replyPacket.ip[2] = 254;
  replyPacket.ip[3] = 255;
  // uint8_t ip_bytes[] = {192, 168, 0, 96};
  // memcpy(replyPacket.ip, ip_bytes, 4);

  // 2. Port (Correctly converted to network byte order)
  replyPacket.port = 0x1936;

  // 3. & 4. VersionInfo (Example: Firmware version 1.5 - you should replace this with your actual version)
  replyPacket.versionInfo[0] = 0x01; // Hi byte
  replyPacket.versionInfo[1] = 0x05; // Lo byte

  // 5. & 6. NetSwitch & SubSwitch
  replyPacket.netSwitch = m_net;
  replyPacket.subSwitch = m_subnet;

  // 7. & 8. Oem (Example: OEM code 0x2923 - replace with your assigned OEM code)
  replyPacket.oem = htons(0x0000);

  // 9. UbeaVersion (Example: UBEA not supported)
  replyPacket.ubeaVersion = 0;

  // 10. Status1 (Correct bit settings)
  replyPacket.status = 0x00;
  replyPacket.status |= 0b00000000; // Bit ?: RDM supported
  replyPacket.status |= 0b00000000; // Bit ?: Not a boot loader
  replyPacket.status |= 0b00100000; // Bit ?: Port-address programming authority is network
  replyPacket.status |= 0b11000000; // Bits ?: Indicators in normal mode

  // 11. & 12. EstaMan (Example: ESTA Manufacturer Code 0x7FF0 - replace with your assigned code)
  replyPacket.estaMan = htons(0x0000);

  // 13. & 14. ShortName & LongName (Ensure null-termination)
  std::string shortName = "GM ArtNet Node";
  std::string longName = "Gaston Morixe ArtNet";
  std::strncpy(reinterpret_cast<char *>(replyPacket.shortName.data()), shortName.c_str(), replyPacket.shortName.size());
  // replyPacket.shortName.back() = '\0'; // Ensure null-termination
  std::strncpy(reinterpret_cast<char *>(replyPacket.longName.data()), longName.c_str(), replyPacket.longName.size());
  // replyPacket.longName.back() = '\0'; // Ensure null-termination

  // 15. NodeReport (Ensure null-termination and correct format)
  std::string nodeReport = "#0001 [0000] Power On Tests successful";
  std::strncpy(reinterpret_cast<char *>(replyPacket.nodeReport.data()), nodeReport.c_str(), replyPacket.nodeReport.size());
  replyPacket.nodeReport.back() = '\0'; // Ensure null-termination

  // 16. NumPorts (Example: 1 output port)
  replyPacket.numPorts = htons(1);

  // 17. PortTypes (Example: Port 0 is DMX512 output)
  replyPacket.portType[0] = 0x80; // Output
  replyPacket.portType[1] = 0x00; // Unused
  replyPacket.portType[2] = 0x00; // Unused
  replyPacket.portType[3] = 0x00; // Unused

  // 18. GoodInputA (Example: No input, all bits cleared)
  replyPacket.goodInputA.fill(0x08);

  // 19. GoodOutputA (Example: DMX outputting, merging not active, not LTP)
  replyPacket.goodOutputA[0] = 0x80; // DMX Outputting
  replyPacket.goodOutputA[1] = 0x00;
  replyPacket.goodOutputA[2] = 0x00;
  replyPacket.goodOutputA[3] = 0x00;

  // 20. SwIn (Example: All set to universe 0)
  replyPacket.swIn.fill(0);

  // 21. SwOut (Example: All set to universe 0)
  replyPacket.swOut.fill(0);

  // 22. AcnPriority (Example: sACN Priority 100)
  replyPacket.acnPriority.fill(100);

  // 23. SwMacro (Example: No macros active)
  replyPacket.swMacro.fill(0);

  // 24. SwRemote (Example: No remote triggers active)
  replyPacket.swRemote.fill(0);

  // 25. Style (Example: StNode)
  replyPacket.style = 0x00; // StNode

  // 26. MAC (Example: 00:26:4D:00:11:22 - replace with your device's MAC)
  replyPacket.mac[0] = 0x00;
  replyPacket.mac[1] = 0x00;
  replyPacket.mac[2] = 0x00;
  replyPacket.mac[3] = 0x00;
  replyPacket.mac[4] = 0x00;
  replyPacket.mac[5] = 0x00;

  // 27. BindIp (Example: Same as device IP if not using binding)
  // replyPacket.bindIp[0] = replyPacket.ip[0];
  // replyPacket.bindIp[1] = replyPacket.ip[1];
  // replyPacket.bindIp[2] = replyPacket.ip[2];
  // replyPacket.bindIp[3] = replyPacket.ip[3];
  std::memcpy(replyPacket.bindIp, replyPacket.ip, 4);

  // 28. BindIndex (Example: 1 if not using binding)
  replyPacket.bindIndex = 1;

  // 29. Status2 (Correct bit settings)
  replyPacket.status2 = 0x00;
  replyPacket.status2 |= 0b00000000; // Web config, 15-bit, RDM, sACN

  // 30. GoodOutputB (Example: RDM disabled, continuous output)
  replyPacket.goodOutputB.fill(0x01);

  // 31. Status3 (Example: hold last state)
  replyPacket.status3 = 0x00; // Hold last state

  // 32. DefaultResponder (Example)
  replyPacket.defaultResponder.fill(0x00);

  // 33. User (Example: Not used)
  replyPacket.userHi = 0;
  replyPacket.userLo = 0;

  // 34. Refresh Rate (Example: Not used)
  replyPacket.refreshRateHi = 0;
  replyPacket.refreshRateLo = 0;

  // 35. Background Queue Policy
  replyPacket.backgroundQueuePolicy = 0;

  // 36. Filler (Not in ArtPollReply) - Removed

  // Calculate packet size
  size_t packetSize = sizeof(ArtPollReplyPacket);

  // Resize packet and copy data
  std::vector<uint8_t> packet(packetSize);
  std::memcpy(packet.data(), &replyPacket, packetSize);

  // Debug print replyPacket bytes
  Logger::debug("ArtPollReply packet bytes:");
  for (size_t i = 0; i < sizeof(replyPacket); i++) {
    if (i % 16 == 0)
      std::cout << std::endl << std::hex << std::setw(4) << std::setfill('0') << i << ": ";
    std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(reinterpret_cast<uint8_t *>(&replyPacket)[i]) << " ";
  }
  std::cout << std::endl;

  // Debug print final packet bytes
  Logger::debug("Final packet bytes:");
  for (size_t i = 0; i < packet.size(); i++) {
    if (i % 16 == 0)
      std::cout << std::endl << std::hex << std::setw(4) << std::setfill('0') << i << ": ";
    std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(packet[i]) << " ";
  }
  std::cout << std::endl;

  // Send the packet
  sendPacket(packet, destIP, destPort);

  Logger::info("--> Sent ArtPollReply to: ", destIP, ":", destPort);
}

void ArtNetController::registerDataCallback(DataCallback callback) {
  std::lock_guard<std::mutex> lock(m_dataMutex);

  m_dataCallback = callback;

  // Enable or disable receiving based on callback presence
  // m_enableReceiving = static_cast<bool>(callback);
}

bool ArtNetController::prepareArtDmxPacket(uint16_t universe, const uint8_t *data, size_t length, std::vector<uint8_t> &packet) {
  if (length > ARTNET_MAX_DMX_SIZE) {
    Logger::error("DMX data exceeds maximum size (", ARTNET_MAX_DMX_SIZE, " bytes)");
    return false;
  }

  // Calculate total packet size:
  // ID(8) + OpCode(2) + ProtVer(2) + Sequence(1) + Physical(1) + SubUni(1) +
  // Net(1) + Length(2) + Data
  size_t packetSize = 18 + length;
  packet.resize(packetSize, 0); // Initialize with zeros

  // 1. Setup Header
  ArtHeader header(OpCode::OpDmx); // This is 0x5000
  std::memcpy(packet.data(), &header, sizeof(ArtHeader));
  size_t offset = sizeof(ArtHeader);

  // 2. Add Protocol Version (14) in big-endian
  uint16_t version = htons(14); // Protocol version 14
  std::memcpy(packet.data() + offset, &version, sizeof(uint16_t));
  offset += sizeof(uint16_t);

  // 3. Sequence number
  packet[offset++] = static_cast<uint8_t>(m_seqNumber++ & 0xFF);

  // Rest remains the same...
  // 4. Physical
  packet[offset++] = 0;

  // 5. SubUni (low byte of 15-bit Port-Address)
  packet[offset++] = (m_subnet << 4) | (m_universe & 0x0F);

  // 6. Net (high byte of 15-bit Port-Address)
  packet[offset++] = m_net & 0x7F;

  // 7. Length in big-endian
  uint16_t lengthBE = htons(static_cast<uint16_t>(length));
  memcpy(packet.data() + offset, &lengthBE, sizeof(uint16_t));
  offset += sizeof(uint16_t);

  // 8. DMX data
  std::memcpy(packet.data() + offset, data, length);
  return true;
}

bool ArtNetController::prepareArtPollPacket(std::vector<uint8_t> &packet) {
  ArtPollPacket pollPacket;

  packet.resize(sizeof(pollPacket));
  std::memcpy(packet.data(), &pollPacket, sizeof(pollPacket));
  return true;
}

bool ArtNetController::sendPacket(const std::vector<uint8_t> &packet, const std::string &address, int port) {
  if (!m_isRunning || !m_networkInterface) {
    Logger::error("Not Running or Interface not initialized");
    return false;
  }
  Logger::debug("sendPacket, packet.size: ", packet.size());

  if (address.empty() && port == 0) {
    if (!m_networkInterface->sendPacket(packet, m_broadcastAddress, m_port)) {
      Logger::error("Error sending packet");
      return false;
    }
  } else {
    if (!m_networkInterface->sendPacket(packet, address, port)) {
      Logger::error("Error sending packet to specific address");
      return false;
    }
  }
  return true;
}

void ArtNetController::receivePackets() {
  Logger::info("receivePackets thread started. bind address: ", m_bindAddress, " port: ", m_port);
  std::vector<uint8_t> buffer(NetworkInterface::MAX_PACKET_SIZE); // Large buffer for incoming packets.

  while (m_isRunning) {
    sockaddr_in senderAddr;
    socklen_t addrLen = sizeof(senderAddr);
    // No MSG_DONTWAIT flag
    int bytesReceived =
        recvfrom(m_networkInterface->getSocket(), buffer.data(), buffer.size(), 0, (struct sockaddr *)&senderAddr, &addrLen);

    Logger::debug("receivePackets, bytesReceived: ", bytesReceived, " buffer.size: ", buffer.size());

    if (bytesReceived > 0) {
      if (static_cast<size_t>(bytesReceived) <= buffer.size()) {
        handleArtPacket(buffer.data(), static_cast<int>(bytesReceived), senderAddr);
      } else {
        Logger::error("Invalid bytesReceived value, ignoring packet");
      }
    } else if (bytesReceived < 0) {
      // Handle error if needed, but don't log if it's just a timeout
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        Logger::error("Error receiving data: ", strerror(errno));
      }
    } else if (bytesReceived < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // Timeout occurred, no data received. This is normal.
        // Check m_isRunning and continue the loop.
        if (!m_isRunning) {
          break;
        }
        continue;
      } else if (errno == EINTR) {
        // Interrupted by a signal, likely SIGINT or SIGTERM
        if (!m_isRunning) {
          break; // Exit the loop if we're supposed to stop
        }
      } else {
        // Other error
        Logger::error("Error receiving data: ", strerror(errno));
      }
    }
  }
}

void ArtNetController::handleArtPacket(const uint8_t *buffer, int size, sockaddr_in senderAddr) {
  if (size < ARTNET_HEADER_SIZE) {
    Logger::debug("handleArtPacket: invalid size");
    return; // Ignore invalid packets
  }

  ArtHeader header(OpCode::OpPoll); // Dummy OpCode, it will be overwritten.
  std::memcpy(&header, buffer, ARTNET_HEADER_SIZE);

  // validate id (should be always "Art-Net")
  if (std::strncmp(reinterpret_cast<const char *>(header.id.data()), "Art-Net", 8) != 0) {
    Logger::error("Invalid Art-Net ID");
    return;
  }

  // uint16_t opcode = ntohs(header.opcode);
  uint16_t opcode = header.opcode; // Already in little-endian

  // Dmx
  if (opcode == static_cast<uint16_t>(OpCode::OpDmx)) {
    Logger::debug("handleArtPacket opcode: OpDmx ", opcode);
    handleArtDmx(buffer, size);

    // Poll
  } else if (opcode == static_cast<uint16_t>(OpCode::OpPoll)) {
    Logger::info("handleArtPacket opcode: OpPoll ", opcode);
    handleArtPoll(buffer, size, senderAddr);

    // PollReply
  } else if (opcode == static_cast<uint16_t>(OpCode::OpPollReply)) {
    Logger::info("handleArtPacket opcode: OpPollReply ", opcode);
    handleArtPollReply(buffer, size);
  } else {

    // Unhandled opcode
    Logger::error("handleArtPacket opcode: NOT HANDLED ", opcode);
  }

  // TODO: Handle more opcodes as needed
}

void ArtNetController::handleArtDmx(const uint8_t *buffer, int size) {
  if (size < ARTNET_HEADER_SIZE + 4)
    return;

  // Interpret buffer as an ArtDmxPacket
  const ArtDmxPacket *dmxPacket = reinterpret_cast<const ArtDmxPacket *>(buffer);

  uint16_t packetUniverse = ntohs(dmxPacket->universe); // Convert from network byte order
  uint16_t dmxLength = ntohs(dmxPacket->length);        // Convert from network byte order
  uint8_t net = (packetUniverse >> 12) & 0x7F;
  uint8_t subnet = (packetUniverse >> 8) & 0xF;
  uint8_t uni = packetUniverse & 0xF;

  // Filter packets based on universe addressing
  if (net != m_net || subnet != m_subnet || uni != m_universe) {
    return;
  }

  // Check if the data callback is set and invoke it
  if (m_dataCallback) {
    // TODO: Rename it to dataDMXCallback
    m_dataCallback(packetUniverse, dmxPacket->data,
                   dmxLength); // Access `data` directly
  }
}

void ArtNetController::handleArtPoll([[maybe_unused]] const uint8_t *buffer, int size, sockaddr_in senderAddr) {
  if (size < ARTNET_HEADER_SIZE + 2) {
    Logger::error("handleArtPoll: Invalid ArtPollPacket size: ", size);
    return;
  }

  ArtPollPacket pollPacket;
  std::memset(&pollPacket, 0, sizeof(pollPacket));
  std::memcpy(&pollPacket, buffer, std::min((size_t)size, sizeof(pollPacket)));

  Logger::debug("Received Poll Packet");
  sendPollReply(buffer, senderAddr);
}

void ArtNetController::handleArtPollReply(const uint8_t *buffer, int size) {
  if (size < 207) {
    Logger::error("handleArtPollReply: Packet size is less than the minimum 207 bytes");
    return;
  }

  // Check if the packet is an ArtPollReply
  if (std::strncmp(reinterpret_cast<const char *>(buffer), "Art-Net", 8) != 0) {
    Logger::error("handleArtPollReply: Not an ArtPollReply packet");
    return;
  }

  // Assuming a packet of 207 bytes or more, we can safely extract fields up to 'MAC Lo'
  // Extract essential fields manually, without casting the entire buffer
  size_t offset = ARTNET_HEADER_SIZE;

  std::array<uint8_t, 4> ip;
  std::memcpy(ip.data(), buffer + offset, sizeof(ip));
  offset += sizeof(ip);

  uint16_t port;
  std::memcpy(&port, buffer + offset, sizeof(port));
  offset += sizeof(port);

  // Convert port to host byte order
  port = ntohs(port);

  // Now, you can use the extracted IP and port for logging or further processing
  Logger::debug("Received ArtPollReply packet from: ", static_cast<int>(ip[0]), ".", static_cast<int>(ip[1]), ".", static_cast<int>(ip[2]),
                ".", static_cast<int>(ip[3]), ":", port);
}
} // namespace ArtNet
