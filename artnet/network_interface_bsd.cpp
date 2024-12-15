#include "network_interface_bsd.h"

#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace ArtNet {
NetworkInterfaceBSD::NetworkInterfaceBSD() : m_recvBuffer(MAX_PACKET_SIZE) {}

bool NetworkInterfaceBSD::createSocket(const std::string &bindAddress,
                                       int port) {
  m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  m_bindAddress = bindAddress;
  m_port = port;
  if (m_socket == -1) {
    std::cerr << "ArtNet: Error creating socket." << std::endl;
    return false;
  }

  // Allow socket to reuse address
  int enable = 1;
  if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) <
      0) {
    std::cerr << "ArtNet: Failed to set socket to reuse address" << std::endl;
    return false;
  }

  int broadcastEnable = 1;
  if (setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable,
                 sizeof(broadcastEnable)) < 0) {
    std::cerr << "ArtNet: Failed to set socket to broadcast" << std::endl;
    return false;
  }

  // Allow multicast
  int loop = 0;
  if (setsockopt(m_socket, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) <
      0) {
    std::cerr << "ArtNet: Failed to set socket to allow multicast loopback"
              << std::endl;
    return false;
  }

  // Set socket to non-blocking
  int flags = fcntl(m_socket, F_GETFL, 0);
  if (flags == -1) {
    std::cerr << "ArtNet: Error getting socket flags" << std::endl;
    return false;
  }
  if (fcntl(m_socket, F_SETFL, flags | O_NONBLOCK) == -1) {
    std::cerr << "ArtNet: Error setting socket to non-blocking" << std::endl;
    return false;
  }
  return true;
}

bool NetworkInterfaceBSD::bindSocket() {
  sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(m_port);
  addr.sin_addr.s_addr = inet_addr(m_bindAddress.c_str());

  std::cout << "ArtNet: Binding socket:" << std::endl
            << "  Address: " << inet_ntoa(addr.sin_addr) << std::endl
            << "  Port: " << ntohs(addr.sin_port) << std::endl
            << "  Family: " << (addr.sin_family == AF_INET ? "IPv4" : "Other")
            << std::endl
            << "  ------- " << std::endl
            << std::endl;

  if (bind(m_socket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == -1) {
    std::cerr << "ArtNet: Error binding socket to address: " << m_bindAddress
              << ":" << m_port << ". " << strerror(errno) << std::endl;
    return false;
  }

  return true;
}

bool NetworkInterfaceBSD::sendPacket(const std::vector<uint8_t> &packet,
                                     const std::string &address, int port) {
  if (m_socket == -1) {
    std::cerr << "ArtNet: Socket not initialized" << std::endl;
    return false;
  }

  sockaddr_in broadcastAddr;
  broadcastAddr.sin_family = AF_INET;
  broadcastAddr.sin_port = htons(port);
  broadcastAddr.sin_addr.s_addr = inet_addr(address.c_str());

  std::cout << "ArtNet: Sending packet:" << std::endl
            << "  Destination: " << inet_ntoa(broadcastAddr.sin_addr)
            << std::endl
            << "  Port: " << ntohs(broadcastAddr.sin_port) << std::endl
            << "  Packet size: " << packet.size() << " bytes" << std::endl;

  ssize_t bytesSent = sendto(m_socket, packet.data(), packet.size(), 0,
                             reinterpret_cast<sockaddr *>(&broadcastAddr),
                             sizeof(broadcastAddr));

  if (bytesSent == -1) {
    std::cerr << "ArtNet: NetworkInterfaceBSD: Error sending packet: "
              << strerror(errno) << std::endl;
    return false;
  }

  return true;
}

int NetworkInterfaceBSD::receivePacket(std::vector<uint8_t> &buffer) {
  sockaddr_in senderAddr;
  socklen_t addrLen = sizeof(senderAddr);

  ssize_t bytesReceived =
      recvfrom(m_socket, m_recvBuffer.data(), m_recvBuffer.size(), 0,
               reinterpret_cast<sockaddr *>(&senderAddr), &addrLen);
  if (bytesReceived == -1) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      std::cerr << "ArtNet (BSD): Error receiving data: " << strerror(errno)
                << std::endl;
    }
    return 0; // Non-blocking socket returns 0 if no data
  }

  std::cout << "ArtNet (BSD): bytes received: " << bytesReceived << std::endl;
  buffer.assign(m_recvBuffer.begin(), m_recvBuffer.begin() + bytesReceived);
  std::cout << "ArtNet (BSD): receivePacket, buffer.size: " << buffer.size()
            << std::endl;

  char senderIP[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &(senderAddr.sin_addr), senderIP, INET_ADDRSTRLEN);
  std::cout << "ArtNet (BSD): Packet received:" << std::endl
            << "  From: " << senderIP << std::endl
            << "  Port: " << ntohs(senderAddr.sin_port) << std::endl
            << "  Bytes received: " << bytesReceived << std::endl;

  return static_cast<int>(bytesReceived);
}
void NetworkInterfaceBSD::closeSocket() {
  if (m_socket != -1) {
    close(m_socket);
    m_socket = -1;
  }
}

} // namespace ArtNet
