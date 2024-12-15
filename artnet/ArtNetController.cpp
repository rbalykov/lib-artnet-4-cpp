#include "ArtNetController.h"
#include "artnet_types.h"
#include <cstring>
#include <iostream>
// #include <stdexcept>
// #include <system_error>
#include <thread>

#ifdef __APPLE__
#include "network_interface_bsd.h"
#else
#include "network_interface_linux.h"
#endif

namespace ArtNet {

ArtNetController::ArtNetController()
    : m_port(ARTNET_PORT), m_net(0), m_subnet(0), m_universe(0),
      m_isRunning(false), m_seqNumber(0), m_dataCallback(nullptr),
      m_isConfigured(false) {}

ArtNetController::~ArtNetController() { stop(); }

bool ArtNetController::configure(const std::string &bindAddress, int port,
                                 uint8_t net, uint8_t subnet, uint8_t universe,
                                 const std::string &broadcastAddress) {
  if (isRunning()) {
    std::cerr << "ArtNet: Cannot configure while running." << std::endl;
    return false;
  }

  // Store configuration
  m_bindAddress = bindAddress;
  m_port = port;
  m_net = net;
  m_subnet = subnet;
  m_universe = universe;
  m_broadcastAddress =
      broadcastAddress.empty() ? "255.255.255.255" : broadcastAddress;
  m_isConfigured = true;

  return true;
}

bool ArtNetController::start() {
  if (!m_isConfigured) {
    std::cerr << "ArtNet: Controller not configured, call configure() first"
              << std::endl;
    return false;
  }
  if (m_isRunning) {
    std::cerr << "ArtNet: Already running" << std::endl;
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

void ArtNetController::stop() {
  if (m_isRunning) {
    m_isRunning = false;
    if (m_receiveThread.joinable()) {
      m_receiveThread.join();
    }
    m_networkInterface->closeSocket();
  }
}

bool ArtNetController::isRunning() const { return m_isRunning; }

bool ArtNetController::setDmxData(uint16_t universe,
                                  const std::vector<uint8_t> &data) {
  if (data.size() > ARTNET_MAX_DMX_SIZE) {
    std::cerr << "ArtNet: DMX data exceeds max size" << std::endl;
    return false;
  }
  if (universe != m_universe) {
    return false;
  }

  std::lock_guard<std::mutex> lock(m_dataMutex);
  m_dmxData = data;
  return true;
}

bool ArtNetController::setDmxData(uint16_t universe, const uint8_t *data,
                                  size_t length) {
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

    if (!prepareArtDmxPacket(m_universe, m_dmxData.data(), m_dmxData.size(),
                             packet))
      return false;
  }

  return sendPacket(packet);
}

bool ArtNetController::sendPoll() {
  std::vector<uint8_t> packet;

  if (!prepareArtPollPacket(packet))
    return false;

  return sendPacket(packet);
}

void ArtNetController::registerDataCallback(DataCallback callback) {
  // m_dataCallback = callback;
  std::lock_guard<std::mutex> lock(m_dataMutex);

  m_dataCallback = callback;

  // Enable or disable receiving based on callback presence
  m_enableReceiving = static_cast<bool>(callback);
}

bool ArtNetController::prepareArtDmxPacket(uint16_t universe,
                                           const uint8_t *data, size_t length,
                                           std::vector<uint8_t> &packet) {
  if (length > ARTNET_MAX_DMX_SIZE) {
    std::cerr << "ArtNet: DMX data exceeds maximum size ("
              << ARTNET_MAX_DMX_SIZE << " bytes)." << std::endl;
    return false;
  }

  ArtDmxPacket dmxPacket;

  // Header
  // dmxPacket.header = ArtHeader(OpCode::OpDmx);

  // Packet Specific Data
  dmxPacket.sequence = static_cast<uint8_t>(m_seqNumber++);
  dmxPacket.physical = 0;
  // uint8_t net = m_net & 0x7F;
  // uint8_t subnet = m_subnet & 0xF;
  // uint8_t uni = m_universe & 0xF;
  // dmxPacket.universe = static_cast<uint16_t>((net << 12) | (subnet << 8) |
  // uni);
  uint8_t net = (m_net & 0x7F);      // Extract 7-bit Net
  uint8_t subnet = (m_subnet & 0xF); // Extract 4-bit SubNet
  uint8_t uni = (m_universe & 0xF);  // Extract 4-bit Universe
  dmxPacket.universe = htons((net << 8) | (subnet << 4) | uni);
  dmxPacket.length = static_cast<uint16_t>(length);

  if (length > ARTNET_MAX_DMX_SIZE) {
    std::cerr << "ArtNet: DMX data exceeds max size" << std::endl;
    return false;
  }

  size_t packetSize = 18 + length; // Header + DMX data
  packet.resize(packetSize);

  // packet.resize(ARTNET_HEADER_SIZE + 4 + length);

  // copy the struct to the output vector
  // std::memcpy(packet.data(), &dmxPacket, ARTNET_HEADER_SIZE + 4);
  // std::memcpy(packet.data() + ARTNET_HEADER_SIZE + 4, data, length);

  // Copy header and DMX data
  std::memcpy(packet.data(), &dmxPacket, 18);    // Copy fixed fields
  std::memcpy(packet.data() + 18, data, length); // Copy DMX data

  std::cout << "ArtNet: prepareArtDmxPacket, packet.size: " << packet.size()
            << std::endl;
  return true;
}

bool ArtNetController::prepareArtPollPacket(std::vector<uint8_t> &packet) {
  ArtPollPacket pollPacket;

  packet.resize(sizeof(pollPacket));

  // copy the struct to the output vector
  std::memcpy(packet.data(), &pollPacket, sizeof(pollPacket));
  return true;
}

bool ArtNetController::sendPacket(const std::vector<uint8_t> &packet) {
  if (!m_isRunning || !m_networkInterface) {
    std::cerr << "ArtNet: Not Running or Interface not initialized"
              << std::endl;
    return false;
  }
  std::cout << "ArtNet: sendPacket, packet.size: " << packet.size()
            << std::endl;

  if (!m_networkInterface->sendPacket(packet, m_broadcastAddress, m_port)) {
    std::cerr << "ArtNet: Error sending packet" << std::endl;
    return false;
  }
  return true;
}

void ArtNetController::receivePackets() {
  std::cout << "ArtNet: receivePackets thread started. bind address: "
            << m_bindAddress << " port: " << m_port << std::endl;
  std::vector<uint8_t> buffer(
      NetworkInterface::MAX_PACKET_SIZE); // Large buffer for incoming packets.

  while (m_isRunning) {
    int bytesReceived = m_networkInterface->receivePacket(buffer);
    std::cout << "ArtNet: receivePackets, bytesReceived: " << bytesReceived
              << "buffer.size: " << buffer.size() << std::endl;
    if (bytesReceived > 0) {
      std::cout << "ArtNet: Received " << bytesReceived << " bytes."
                << std::endl;
      if (bytesReceived > 0 &&
          static_cast<size_t>(bytesReceived) <= buffer.size()) {
        handleArtPacket(buffer.data(), static_cast<int>(bytesReceived));
      } else {
        std::cerr << "ArtNet: Invalid bytesReceived value, ignoring packet"
                  << std::endl;
      }
    }

    // Add a small delay
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

void ArtNetController::handleArtPacket(const uint8_t *buffer, int size) {
  if (size < ARTNET_HEADER_SIZE) {
    return; // Ignore invalid packets
  }

  std::cout << "ArtNetController: handleArtPacket" << std::endl;

  ArtHeader header(OpCode::OpPoll); // Dummy OpCode, it will be overwritten.
  std::memcpy(&header, buffer, ARTNET_HEADER_SIZE);

  // validate id (should be always "Art-Net")
  if (std::strncmp(reinterpret_cast<const char *>(header.id.data()), "Art-Net",
                   8) != 0) {
    return;
  }

  uint16_t opcode = ntohs(header.opcode);

  if (opcode == static_cast<uint16_t>(OpCode::OpDmx)) {
    handleArtDmx(buffer, size);
  } else if (opcode == static_cast<uint16_t>(OpCode::OpPoll)) {
    handleArtPoll(buffer, size);
  } else if (opcode == static_cast<uint16_t>(OpCode::OpPollReply)) {
    handleArtPollReply(buffer, size);
  }
  // Add more opcodes as needed
}

void ArtNetController::handleArtDmx(const uint8_t *buffer, int size) {
  if (size < ARTNET_HEADER_SIZE + 4)
    return;

  // Interpret buffer as an ArtDmxPacket
  const ArtDmxPacket *dmxPacket =
      reinterpret_cast<const ArtDmxPacket *>(buffer);

  uint16_t packetUniverse =
      ntohs(dmxPacket->universe); // Convert from network byte order
  uint16_t dmxLength =
      ntohs(dmxPacket->length); // Convert from network byte order
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

void ArtNetController::handleArtPoll([[maybe_unused]] const uint8_t *buffer,
                                     int size) {
  if (size < static_cast<int>(sizeof(ArtPollPacket)))
    return;

  std::cout << "ArtNet: Recieved Poll Packet" << std::endl;
  sendPoll();
}

void ArtNetController::handleArtPollReply(const uint8_t *buffer, int size) {
  if (size < static_cast<int>(sizeof(ArtPollReplyPacket)))
    return;

  const ArtPollReplyPacket *pollReplyPacket =
      reinterpret_cast<const ArtPollReplyPacket *>(buffer);
  std::cout << "ArtNet: Received Poll Reply packet from: "
            << static_cast<int>(pollReplyPacket->ip[0]) << "."
            << static_cast<int>(pollReplyPacket->ip[1]) << "."
            << static_cast<int>(pollReplyPacket->ip[2]) << "."
            << static_cast<int>(pollReplyPacket->ip[3]) << ":"
            << (pollReplyPacket->port) << std::endl;
}
} // namespace ArtNet
