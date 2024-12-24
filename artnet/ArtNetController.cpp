#include "ArtNetController.h"
#include "artnet_types.h"
#include "logging.h"
#include "utils.h"

#include <chrono>
#include <cstring>
#include <random>
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

  // Start receiving thread only if enabled
  if (m_enableReceiving) {
    m_receiveThread = std::thread(&ArtNetController::receivePackets, this);
  }

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
#ifdef __linux__
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(0, &cpuset); // Bind to first CPU core
  pthread_setaffinity_np(m_processorThread.native_handle(), sizeof(cpu_set_t), &cpuset);
#endif
}

void ArtNetController::stop() {
  m_isRunning = false;

  // Stop frame processor thread
  if (m_processorThread.joinable()) {
    m_processorThread.join();
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

bool ArtNetController::setDmxData(uint16_t universe, const std::vector<uint8_t> &data) {
  if (data.size() > ARTNET_MAX_DMX_SIZE) {
    Logger::error("DMX data exceeds max size");
    return false;
  }
  if (universe != m_universe) {
    return false;
  }

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

void ArtNetController::sendPollReply(const uint8_t *buffer, int size) {
  Logger::debug("sendPollReply: size: ", size);

  const ArtPollPacket *pollPacket = reinterpret_cast<const ArtPollPacket *>(buffer);
  std::vector<uint8_t> packet;

  // Create the ArtPollReply packet
  ArtPollReplyPacket replyPacket;

  // Populate the ArtPollReply packet
  replyPacket.ip = utils::parseIP(m_bindAddress);
  replyPacket.port = htons(m_port);
  replyPacket.versionInfo[0] = 0; // Firmware Version Hi
  replyPacket.versionInfo[1] = 0; // Firmware Version Lo
  replyPacket.netSwitch = m_net;
  replyPacket.subSwitch = m_subnet;
  replyPacket.oem = 0;                                                       // TODO: Make configurable
  replyPacket.ubeaVersion = 0;                                               // UBEA version
  replyPacket.status = 0x01;                                                 // Status: Ok.  (TODO: Use enum/define)
  replyPacket.estaMan = 0x00;                                                // TODO: Should be a value set by ESTA
  std::string shortName = "GM ArtNet Node";                                  // TODO: Make configurable.
  std::string longName = "Gaston Morixe ArtNet Node with awesome functions"; // TODO: Make Configurable.

  std::memcpy(replyPacket.shortName.data(), shortName.c_str(), std::min((size_t)shortName.size(), (size_t)replyPacket.shortName.size()));
  std::memcpy(replyPacket.longName.data(), longName.c_str(), std::min((size_t)longName.size(), (size_t)replyPacket.longName.size()));
  std::memset(replyPacket.nodeReport.data(), 0, replyPacket.nodeReport.size()); // TODO: Add node information here
  replyPacket.numPorts = 1;                                                     // TODO: Make configurable.
  replyPacket.portType.fill(0);
  replyPacket.goodOutputA.fill(0);
  replyPacket.goodInputA.fill(0);
  replyPacket.swIn.fill(0);
  replyPacket.swOut.fill(0);
  replyPacket.acnPriority.fill(0);
  replyPacket.swMacro.fill(0);
  replyPacket.swRemote.fill(0);

  packet.resize(sizeof(replyPacket));
  std::memcpy(packet.data(), &replyPacket, sizeof(replyPacket));

  // Generate random delay
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 1000);

  // get source address from poll packet to reply unicast
  sockaddr_in senderAddr;
  socklen_t addrLen = sizeof(senderAddr);
  getpeername(m_networkInterface->getSocket(), (sockaddr *)&senderAddr, &addrLen);
  std::string destIP = utils::formatIP(reinterpret_cast<const uint8_t *>(&senderAddr.sin_addr.s_addr), 4);
  int destPort = ntohs(senderAddr.sin_port);
  Logger::debug("Sending ArtPollReply to: ", destIP, ":", destPort);

  std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));
  sendPacket(packet, destIP, destPort);
}

void ArtNetController::registerDataCallback(DataCallback callback) {
  // m_dataCallback = callback;
  std::lock_guard<std::mutex> lock(m_dataMutex);

  m_dataCallback = callback;

  // Enable or disable receiving based on callback presence
  m_enableReceiving = static_cast<bool>(callback);
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

  // 2. Sequence number
  packet[offset++] = static_cast<uint8_t>(m_seqNumber++ & 0xFF);

  // 3. Physical
  packet[offset++] = 0;

  // 4. SubUni (low byte of 15-bit Port-Address)
  packet[offset++] = (m_subnet << 4) | (m_universe & 0x0F);

  // 5. Net (high byte of 15-bit Port-Address)
  packet[offset++] = m_net & 0x7F;

  // 6. Length in big-endian
  uint16_t lengthBE = htons(static_cast<uint16_t>(length));
  memcpy(packet.data() + offset, &lengthBE, sizeof(uint16_t));
  offset += sizeof(uint16_t);

  // 7. DMX data
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
  std::vector<uint8_t> buffer(NetworkInterface::MAX_PACKET_SIZE); // Large buffer for incoming
                                                                  // packets.

  while (m_isRunning) {
    int bytesReceived = m_networkInterface->receivePacket(buffer);
    Logger::debug("receivePackets, bytesReceived: ", bytesReceived, " buffer.size: ", buffer.size());
    if (bytesReceived > 0) {
      Logger::debug("Received ", bytesReceived, " bytes");
      if (bytesReceived > 0 && static_cast<size_t>(bytesReceived) <= buffer.size()) {
        handleArtPacket(buffer.data(), static_cast<int>(bytesReceived));
      } else {
        Logger::error("Invalid bytesReceived value, ignoring packet");
      }
    }

    // Add a small delay only if something was received
    if (bytesReceived <= 0)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

void ArtNetController::handleArtPacket(const uint8_t *buffer, int size) {
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
    Logger::debug("handleArtPacket opcode: OpPoll ", opcode);
    handleArtPoll(buffer, size);

    // PollReply
  } else if (opcode == static_cast<uint16_t>(OpCode::OpPollReply)) {
    Logger::debug("handleArtPacket opcode: OpPollReply ", opcode);
    handleArtPollReply(buffer, size);
  } else {

    // Unhandled opcode
    Logger::debug("handleArtPacket opcode: NOT HANDLED ", opcode);
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
    m_dataCallback(packetUniverse, dmxPacket->data,
                   dmxLength); // Access `data` directly
  }
}

void ArtNetController::handleArtPoll([[maybe_unused]] const uint8_t *buffer, int size) {
  if (size < ARTNET_HEADER_SIZE + 2) {
    Logger::error("handleArtPoll: Invalid ArtPollPacket size: ", size);
    return;
  }

  ArtPollPacket pollPacket;
  std::memset(&pollPacket, 0, sizeof(pollPacket));
  std::memcpy(&pollPacket, buffer, std::min((size_t)size, sizeof(pollPacket)));

  // if (size < static_cast<int>(sizeof(ArtPollPacket))) {
  //   Logger::error("handleArtPoll: Invalid ArtPollPacket size: ", size);
  //   return;
  // }

  // Now you can safely access the fields of pollPacket,
  // knowing that any missing fields beyond the received size will be zero.

  Logger::debug("Received Poll Packet");
  sendPollReply(buffer, size);
}

void ArtNetController::handleArtPollReply(const uint8_t *buffer, int size) {
  if (size < static_cast<int>(sizeof(ArtPollReplyPacket))) {
    Logger::error("handleArtPollReply: Invalid ArtPollReplyPacket size");
    return;
  }

  const ArtPollReplyPacket *pollReplyPacket = reinterpret_cast<const ArtPollReplyPacket *>(buffer);
  Logger::debug("Received Poll Reply packet from: ", static_cast<int>(pollReplyPacket->ip[0]), ".",
                static_cast<int>(pollReplyPacket->ip[1]), ".", static_cast<int>(pollReplyPacket->ip[2]), ".",
                static_cast<int>(pollReplyPacket->ip[3]), ":", pollReplyPacket->port);
}
} // namespace ArtNet
