#include "socket.hpp"

#include <arpa/inet.h>
#include <cstddef>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

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

// error message behavior is similar to C perror() function
Socket::ConnectionResetException::ConnectionResetException(const std::string &prefix)
	: std::runtime_error(prefix != "" ? prefix + ": " + "connection reset by peer" :
					    "connection reset by peer")
{
}

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
	std::string stored_message;
	for (curr = res.get(); curr != nullptr; curr = curr->ai_next)
	{
		m_socket = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);

		if (m_socket == -1)
		{
			stored_message = std::string("socket(): ") + strerror(errno);
			continue;
		}

		rc = fcntl(m_socket, F_SETFL, O_NONBLOCK);
		if (rc == -1)
		{
			stored_message = std::string("fcntl(): ") + strerror(errno);
			close(m_socket);
			continue;
		}

		rc = connect(m_socket, curr->ai_addr, curr->ai_addrlen);
		if (rc == 0)
		{
			m_connected = true;
			break;
		}
		if (rc == -1 && errno != EINPROGRESS)
		{
			stored_message = std::string("connect(): ") + strerror(errno);
			close(m_socket);
			continue;
		}

		break;
	}

	if (curr == nullptr)
	{
		throw std::runtime_error(std::string("Socket(): ") + stored_message);
	}

	const std::string str = ::ntop(curr->ai_addr);

	std::clog << "Connected to " << hostname << " (" << str << ":" << port << ")" << '\n';
}

Socket::Socket(Socket &&other) noexcept
{
	this->~Socket();
	m_socket = std::exchange(other.m_socket, -1);
	m_connected = other.m_connected;
}

Socket &Socket::operator=(Socket &&other) noexcept
{
	if (this != &other)
	{
		this->~Socket();
		m_socket = std::exchange(other.m_socket, -1);
		m_connected = other.m_connected;
	}

	return *this;
}

void Socket::validate_connect() const
{
	int rc;
	int error = 0;
	socklen_t err_len = sizeof error;
	rc = getsockopt(m_socket, SOL_SOCKET, SO_ERROR, &error, &err_len);
	if (rc == -1)
	{
		throw std::runtime_error(std::string("validate_connect(): getsockopt(): ") +
					 strerror(errno));
	}
	if (error != 0)
	{
		// TODO: test if error actually represent valid errno
		throw std::runtime_error(std::string("validate_connect(): connect(): ") +
					 strerror(error));
	}
	m_connected = true;
}

void Socket::send(const std::span<const uint8_t> buffer, size_t &offset) const
{
	size_t total = offset;
	size_t bytesleft = buffer.size() - total;
	ssize_t n = 0;

	while (total < buffer.size())
	{
		n = ::send(m_socket, buffer.data() + total, bytesleft, 0);
		if (n == -1)
		{
			break;
		}
		total += n;
		bytesleft -= n;
	}

	offset = total;

	if (n == -1 && (errno == EWOULDBLOCK || errno == EAGAIN))
	{
		return;
	}

	if (n == -1 && errno == ECONNRESET)
	{
		shutdown(m_socket, SHUT_WR);
		throw ConnectionResetException("send()");
	}

	if (n == -1)
	{
		throw std::runtime_error("send(): " + std::string(strerror(errno)));
	}

	offset = 0;
}

void Socket::recv(const std::span<uint8_t> buffer, size_t &offset) const
{
	size_t len = buffer.size() - offset;
	ssize_t bytes_received = 0;
	size_t bytes_received_total = offset;

	while (len > 0 && (bytes_received = ::recv(m_socket, buffer.data() + bytes_received_total,
						   len, 0)) > 0)
	{
		len -= bytes_received;
		bytes_received_total += bytes_received;
	}

	offset = bytes_received_total;

	if (bytes_received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
	{
		return;
	}

	if (bytes_received < 0)
	{
		throw std::runtime_error("recv(): " + std::string(strerror(errno)));
	}

	// connection closed
	if (bytes_received == 0)
	{
		shutdown(m_socket, SHUT_RD);
		throw ConnectionResetException("recv()");
	}

	offset = 0;
}

bool Socket::connected() const
{
	return m_connected;
}

int Socket::get_fd() const
{
	return m_socket;
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
	auto *reinterpreted_sa = reinterpret_cast<sockaddr *>(&sa);

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
