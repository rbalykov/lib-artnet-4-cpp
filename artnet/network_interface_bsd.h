#pragma once

#include "ArtNetController.h"
#include <string>
#include <vector>

namespace ArtNet
{
class NetworkInterfaceBSD: public NetworkInterface
{
public:
	NetworkInterfaceBSD();
	~NetworkInterfaceBSD() override = default;

	bool createSocket(const std::string &bindAddress, int port) override;
	bool bindSocket() override;
	bool sendPacket(const std::vector<uint8_t> &packet,
			const std::string &address, int port) override;
	int receivePacket(std::vector<uint8_t> &buffer) override;
	void closeSocket() override;
	virtual int getSocket() const override;

private:
	int m_socket = -1;
	std::string m_bindAddress;
	int m_port;
	std::vector<uint8_t> m_recvBuffer;
};
} // namespace ArtNet
