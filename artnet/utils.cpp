#include "utils.h"
#include <sstream>
// #include <iomanip>

namespace ArtNet {
namespace utils {

std::string formatIP(const std::array<uint8_t, 4> &ip) {
  std::stringstream ss;
  ss << static_cast<int>(ip[0]) << "." << static_cast<int>(ip[1]) << "." << static_cast<int>(ip[2]) << "." << static_cast<int>(ip[3]);
  return ss.str();
}

} // namespace utils
} // namespace ArtNet
