#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace ArtNet
{
// Abstract class for network interface ( platform agnostic )
class NetworkInterface
{
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
	virtual int getSocket() const = 0; // Added getSocket
};

} // namespace ArtNet
