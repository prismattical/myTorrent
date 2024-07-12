#include "socket.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

std::string ntop(const sockaddr *sa)
{
	std::string ret;
	switch (sa->sa_family)
	{
	case AF_INET:
		ret.resize(INET_ADDRSTRLEN);
		inet_ntop(sa->sa_family, &(reinterpret_cast<const sockaddr_in *>(sa)->sin_addr),
			  ret.data(), ret.size());
		break;

	case AF_INET6:
		ret.resize(INET6_ADDRSTRLEN);
		inet_ntop(sa->sa_family, &(reinterpret_cast<const sockaddr_in6 *>(sa)->sin6_addr),
			  ret.data(), ret.size());
		break;

	default:

		break;
	}
	return ret;
}

uint16_t get_port(const sockaddr *sa)
{
	switch (sa->sa_family)
	{
	case AF_INET:
		return reinterpret_cast<const sockaddr_in *>(sa)->sin_port;

	case AF_INET6:
		return reinterpret_cast<const sockaddr_in6 *>(sa)->sin6_port;

	default:
		return 0;
		break;
	}
}

int protocol_to_int(const Socket::Protocol protocol) noexcept
{
	switch (protocol)
	{
	case Socket::TCP:
		return SOCK_STREAM;
	case Socket::UDP:
		return SOCK_DGRAM;
	default:
		return -1;
	}
}

int ip_version_to_int(const Socket::IPVersion ip_version) noexcept
{
	switch (ip_version)
	{
	case Socket::IPV4:
		return AF_INET;
	case Socket::IPV6:
		return AF_INET6;
	case Socket::IP_UNSPEC:
		return AF_UNSPEC;
	default:
		return -1;
	}
}

Socket::Socket()
	: m_socket{ -1 }
{
}

Socket::Socket(const std::string &hostname, const std::string &port, const Protocol protocol,
	       const IPVersion ip_version)
{
	int rc;
	struct addrinfo hints;
	struct addrinfo *res_temp;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = ip_version_to_int(ip_version);
	hints.ai_socktype = protocol_to_int(protocol);

	rc = getaddrinfo(hostname.c_str(), port.c_str(), &hints, &res_temp);

	if (rc != 0)
	{
		throw std::runtime_error("getaddrinfo(): " + std::string(gai_strerror(rc)));
	}

	const std::unique_ptr<struct addrinfo, decltype(&freeaddrinfo)> res(res_temp, freeaddrinfo);

	struct addrinfo *curr;
	for (curr = res.get(); curr != nullptr; curr = curr->ai_next)
	{
		m_socket = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);

		if (m_socket == -1)
		{
			std::cerr << "socket(): " << strerror(errno) << '\n';
			continue;
		}

		rc = connect(m_socket, curr->ai_addr, curr->ai_addrlen);
		if (rc == -1)
		{
			std::cerr << "connect(): " << strerror(errno) << '\n';
			close(m_socket);
			continue;
		}

		break;
	}

	if (curr == nullptr)
	{
		throw std::runtime_error("Socket(): failed to connect");
	}

	const std::string str = ::ntop(curr->ai_addr);

	std::clog << "Connected to " << hostname << " (" << str << ":" << port << ")" << '\n';
}

Socket::Socket(Socket &&other) noexcept
{
	this->~Socket();
	m_socket = std::exchange(other.m_socket, -1);
}

Socket &Socket::operator=(Socket &&other) noexcept
{
	if (this != &other)
	{
		this->~Socket();
		m_socket = std::exchange(other.m_socket, -1);
	}

	return *this;
}

void Socket::send_all(const std::vector<unsigned char> &data) const
{
	const unsigned char *buf = data.data();
	size_t total = 0;
	size_t bytesleft = data.size();
	ssize_t n = 0;

	while (total < data.size())
	{
		n = ::send(m_socket, buf + total, bytesleft, 0);
		if (n == -1)
		{
			break;
		}
		total += n;
		bytesleft -= n;
	}

	if (n == -1)
	{
		throw std::runtime_error("send_all(): " + std::string(strerror(errno)));
	}
}

std::vector<unsigned char> Socket::recv_until_close() const
{
	std::vector<unsigned char> data;

	static constexpr size_t buffer_size = 4096;

	static std::array<unsigned char, buffer_size> buffer;

	ssize_t bytes_received;
	while ((bytes_received = ::recv(m_socket, buffer.data(), buffer.size(), 0)) > 0)
	{
		data.insert(data.end(), buffer.begin(), buffer.begin() + bytes_received);
		std::cout << "Received " << bytes_received << " bytes during recv()" << '\n';
	}

	if (bytes_received == -1)
	{
		throw std::runtime_error("recv_until_close(): " + std::string(strerror(errno)));
	}

	return data;
}

std::vector<unsigned char> Socket::recv_some(size_t len) const
{
	std::vector<unsigned char> ret(len);
	ssize_t bytes_received = 0;
	size_t bytes_received_total = 0;
	while (len > 0 &&
	       (bytes_received = recv(m_socket, ret.data() + bytes_received_total, len, 0)) > 0)
	{
		len -= bytes_received;
		bytes_received_total += bytes_received;
	}
	if (len != 0 || bytes_received < 0)
	{
		throw std::runtime_error("recv_some(): " + std::string(strerror(errno)));
	}
	return ret;
}

uint32_t Socket::recv_length() const
{
	uint32_t ret;
	size_t len = sizeof ret;
	ssize_t bytes_received = 0;
	size_t bytes_received_total = 0;
	while (len > 0 && (bytes_received = recv(
				   m_socket, reinterpret_cast<char *>(&ret) + bytes_received_total,
				   len, 0)) > 0)
	{
		len -= bytes_received;
		bytes_received_total += bytes_received;
	}
	if (len != 0 || bytes_received < 0)
	{
		throw std::runtime_error("recv_some(): " + std::string(strerror(errno)));
	}
	return ret;
}

Socket::~Socket()
{
	close(m_socket);
}

std::tuple<std::string, std::string> Socket::get_peer_ip_and_port() const
{
	sockaddr_storage sa;
	socklen_t s = sizeof sa;
	// this var is for readability only
	sockaddr *reinterpreted_sa = reinterpret_cast<sockaddr *>(&sa);

	getpeername(m_socket, reinterpreted_sa, &s);
	return { ::ntop(reinterpreted_sa), std::to_string(ntohs(::get_port(reinterpreted_sa))) };
}

std::string Socket::ntop(uint32_t ip)
{
	in_addr src = { ip };
	std::string ret;
	ret.reserve(INET_ADDRSTRLEN);
	inet_ntop(AF_INET, &src, ret.data(), ret.size());
	return ret;
}
