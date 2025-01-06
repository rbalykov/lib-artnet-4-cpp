#include "network_interface_bsd.h"
#include "logging.h"

#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace ArtNet
{
NetworkInterfaceBSD::NetworkInterfaceBSD() :
		m_recvBuffer(MAX_PACKET_SIZE)
{
}

int NetworkInterfaceBSD::getSocket() const
{
	return m_socket;
}

bool NetworkInterfaceBSD::createSocket(const std::string &bindAddress, int port)
{
	m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	m_bindAddress = bindAddress;
	m_port = port;
	if (m_socket == -1)
	{
		Logger::error("Error creating socket");
		return false;
	}

#ifdef SO_REUSEPORT
	int enableReusePort = 1;
	if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEPORT, &enableReusePort,
			sizeof(int)) < 0)
	{
		Logger::error("Failed to set socket to reuse port");
		return false;
	}
#endif

	// Allow socket to reuse address
	int enable = 1;
	if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int))
			< 0)
	{
		Logger::error("Failed to set socket to reuse address");
		return false;
	}

	int broadcastEnable = 1;
	if (setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable,
			sizeof(broadcastEnable)) < 0)
	{
		Logger::error("Failed to set socket to broadcast");
		return false;
	}

	// Allow multicast
	int loop = 0;
	if (setsockopt(m_socket, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop))
			< 0)
	{
		Logger::error("Failed to set socket to allow multicast loopback");
		return false;
	}

	struct timeval tv;
	tv.tv_sec = 0;       // Timeout in seconds
	tv.tv_usec = 500000; // Timeout in microseconds (0.5 seconds)

	if (setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*) &tv,
			sizeof tv) < 0)
	{
		std::cerr << "ArtNet: Error setting socket timeout" << std::endl; // Or Logger::error in BSD
		return false;
	}

	// Set socket to non-blocking
	// int flags = fcntl(m_socket, F_GETFL, 0);
	// if (flags == -1) {
	//   Logger::error("Error getting socket flags");
	//   return false;
	// }
	// if (fcntl(m_socket, F_SETFL, flags | O_NONBLOCK) == -1) {
	//   Logger::error("Error setting socket to non-blocking");
	//   return false;
	// }

	return true;
}

bool NetworkInterfaceBSD::bindSocket()
{
	// Check port is in use
	sockaddr_in check_addr;
	check_addr.sin_family = AF_INET;
	check_addr.sin_port = htons(m_port);
	check_addr.sin_addr.s_addr = INADDR_ANY;

	int check_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (bind(check_socket, (struct sockaddr*) &check_addr, sizeof(check_addr))
			== 0)
	{
		close(check_socket); // Port is free
	}
	else
	{
		close(check_socket);
		Logger::info("Port already in use, but continuing due to SO_REUSEADDR");
	}

	// Binding
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(m_port);
	addr.sin_addr.s_addr = inet_addr(m_bindAddress.c_str());

	Logger::info("Binding socket:", "\n  Address: ", inet_ntoa(addr.sin_addr),
			"\n  Port: ", ntohs(addr.sin_port), "\n  Family: ",
			(addr.sin_family == AF_INET ? "IPv4" : "Other"));

	if (bind(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1)
	{
		Logger::error("Error binding socket to address: ", m_bindAddress, ":",
				m_port, ". ", strerror(errno));
		return false;
	}

	return true;
}

bool NetworkInterfaceBSD::sendPacket(const std::vector<uint8_t> &packet,
		const std::string &address, int port)
{
	if (m_socket == -1)
	{
		Logger::error("Socket not initialized");
		return false;
	}

	sockaddr_in broadcastAddr;
	broadcastAddr.sin_family = AF_INET;
	broadcastAddr.sin_port = htons(port);
	broadcastAddr.sin_addr.s_addr = inet_addr(address.c_str());

	Logger::debug("Sending packet:", "\n  Destination: ",
			inet_ntoa(broadcastAddr.sin_addr), "\n  Port: ",
			ntohs(broadcastAddr.sin_port), "\n  Packet size: ", packet.size(),
			" bytes");

	ssize_t bytesSent = sendto(m_socket, packet.data(), packet.size(), 0,
			reinterpret_cast<sockaddr*>(&broadcastAddr), sizeof(broadcastAddr));

	if (bytesSent == -1)
	{
		Logger::error("NetworkInterfaceBSD: Error sending packet: ",
				strerror(errno));
		return false;
	}

	return true;
}

int NetworkInterfaceBSD::receivePacket(std::vector<uint8_t> &buffer)
{
	sockaddr_in senderAddr;
	socklen_t addrLen = sizeof(senderAddr);

	ssize_t bytesReceived = recvfrom(m_socket, m_recvBuffer.data(),
			m_recvBuffer.size(), 0, reinterpret_cast<sockaddr*>(&senderAddr),
			&addrLen);
	if (bytesReceived == -1)
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			Logger::error("Error receiving data: ", strerror(errno));
		}
		return 0; // Non-blocking socket returns 0 if no data
	}

	buffer.assign(m_recvBuffer.begin(), m_recvBuffer.begin() + bytesReceived);

	char senderIP[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(senderAddr.sin_addr), senderIP, INET_ADDRSTRLEN);

	Logger::debug("Packet received:", "\n  From: ", senderIP, "\n  Port: ",
			ntohs(senderAddr.sin_port), "\n  Bytes received: ", bytesReceived,
			"\n  Buffer size: ", buffer.size());

	return static_cast<int>(bytesReceived);
}

void NetworkInterfaceBSD::closeSocket()
{
	if (m_socket != -1)
	{
		close(m_socket);
		m_socket = -1;
	}
}

} // namespace ArtNet
