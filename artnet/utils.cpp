#include "utils.h"
#include "logging.h"

#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <sstream>

namespace ArtNet {
namespace utils {

std::string formatIP(const std::array<uint8_t, 4> &ip) {
  std::stringstream ss;
  ss << static_cast<int>(ip[0]) << "." << static_cast<int>(ip[1]) << "." << static_cast<int>(ip[2]) << "." << static_cast<int>(ip[3]);
  return ss.str();
}

std::string formatIP(const uint8_t *data, size_t size) {
  std::stringstream ss;
  if (size == 4) {
    ss << static_cast<int>(data[0]) << "." << static_cast<int>(data[1]) << "." << static_cast<int>(data[2]) << "."
       << static_cast<int>(data[3]);
  }
  return ss.str();
}

std::array<uint8_t, 4> parseIP(std::string const &ipString) {
  std::array<uint8_t, 4> ip{};
  std::stringstream ss(ipString);
  std::string segment;
  int i = 0;
  while (std::getline(ss, segment, '.') && i < 4) {
    try {
      ip[i++] = static_cast<uint8_t>(std::stoi(segment));
    } catch (const std::exception &e) {
      Logger::error("Error parsing IP segment: " + segment + " - " + e.what());
      return std::array<uint8_t, 4>{};
    }
  }
  return ip;
}

std::string ipAddressToString(sockaddr_in ip) {
  char str[INET_ADDRSTRLEN];

  // store this IP address in sa:
  // return inet_pton(AF_INET, "192.0.2.33", &(sa.sin_addr));

  // now get it back and print it
  return inet_ntop(AF_INET, &(ip.sin_addr), str, INET_ADDRSTRLEN);
}

} // namespace utils
} // namespace ArtNet
