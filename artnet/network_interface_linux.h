#pragma once

#include "ArtNetController.h"
#include <string>

namespace ArtNet {
class NetworkInterfaceLinux : public NetworkInterface {
public:
  NetworkInterfaceLinux() = default;
  ~NetworkInterfaceLinux() override = default;

  bool createSocket(const std::string &bindAddress, int port) override;
  bool bindSocket() override;
  bool sendPacket(const std::vector<uint8_t> &packet,
                  const std::string &address, int port) override;
  int receivePacket(std::vector<uint8_t> &buffer) override;
  void closeSocket() override;

private:
  int m_socket = -1;
  std::string m_bindAddress;
  int m_port;
};
} // namespace ArtNet
