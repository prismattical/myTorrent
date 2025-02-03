#include "socket.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <tuple>
#include <unistd.h>
#include <utility>

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

TCPClient::TCPClient(const std::string &hostname, const std::string &port)
{
	connect(hostname, port);
}

void TCPClient::connect(const std::string &hostname, const std::string &port)
{
	this->~TCPClient();

	int rc = 0;
	struct addrinfo hints {};
	struct addrinfo *res_temp = nullptr;

	std::memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	rc = getaddrinfo(hostname.c_str(), port.c_str(), &hints, &res_temp);

	if (rc != 0)
	{
		std::cerr << "getaddrinfo(): " << gai_strerror(rc) << '\n';
		throw std::runtime_error("Failed to connect to server");
	}

	const std::unique_ptr<struct addrinfo, decltype(&freeaddrinfo)> res(res_temp, freeaddrinfo);

	struct addrinfo *curr = nullptr;
	std::string stored_message;
	for (curr = res.get(); curr != nullptr; curr = curr->ai_next)
	{
		m_socket = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);

		if (m_socket == -1)
		{
			std::cerr << "socket(): " << strerror(errno) << '\n';
			continue;
		}

		rc = fcntl(m_socket, F_SETFL, O_NONBLOCK);
		if (rc == -1)
		{
			std::cerr << "fcntl(): " << strerror(errno) << '\n';
			close(m_socket);
			continue;
		}

		rc = ::connect(m_socket, curr->ai_addr, curr->ai_addrlen);

		if (rc == -1 && errno != EINPROGRESS)
		{
			std::cerr << "connect(): " << strerror(errno) << '\n';
			close(m_socket);
			continue;
		}

		break;
	}

	if (curr == nullptr)
	{
		m_socket = -1;
		throw std::runtime_error("Failed to connect to server");
	}

	// const std::string str = ::ntop(curr->ai_addr);
	// std::clog << "Connected to " << hostname << " (" << str << ":" << port << ")" << '\n';
}

TCPClient::TCPClient(TCPClient &&other) noexcept
{
	this->~TCPClient();
	m_socket = std::exchange(other.m_socket, -1);
}

TCPClient &TCPClient::operator=(TCPClient &&other) noexcept
{
	if (this != &other)
	{
		this->~TCPClient();
		m_socket = std::exchange(other.m_socket, -1);
	}

	return *this;
}

bool TCPClient::connect_successful() const
{
	int rc = 0;
	int error = 0;
	socklen_t err_len = sizeof error;
	rc = getsockopt(m_socket, SOL_SOCKET, SO_ERROR, &error, &err_len);
	if (rc == -1)
	{
		std::cerr << "validate_connect(): getsockopt(): " << strerror(errno) << '\n';
		return false;
	}
	if (error != 0)
	{
		std::cerr << "validate_connect(): connect(): " << strerror(error) << '\n';
		return false;
	}
	return true;
}

long TCPClient::send(const std::span<const uint8_t> buffer) const
{
	ssize_t n = ::send(m_socket, buffer.data(), buffer.size(), 0);

	if (n == -1 && (errno == EWOULDBLOCK || errno == EAGAIN))
	{
		// theoretically this method may return 0 in this case
		// because failing because call would block
		// basically means that everything ok but zero bytes were sent
		// and it will spare the caller an if check
		// but I personally find this method more comprehensible this way
		return -1;
	}

	if (n == -1)
	{
		throw std::runtime_error("send() failed");
	}

	return n;
}

long TCPClient::recv(const std::span<uint8_t> buffer) const
{
	size_t len = buffer.size();
	ssize_t n = ::recv(m_socket, buffer.data(), len, 0);

	if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
	{
		return -1;
	}

	if (n < 0)
	{
		throw std::runtime_error("recv() failed");
	}

	return n;
}

long TCPClient::recv2(const std::span<uint8_t> buffer) const
{
	size_t len = buffer.size();
	ssize_t n = ::recv(m_socket, buffer.data(), len, 0);

	if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
	{
		// theoretically this method may return 0 in this case
		// because failing because call would block
		// basically means that everything ok but zero bytes were recved
		// and it will spare the caller an if check
		// but I personally find this method more comprehensible this way
		return -1;
	}

	if (n <= 0)
	{
		throw std::runtime_error("recv() failed");
	}

	return n;
}

int TCPClient::get_fd() const
{
	return m_socket;
}

void TCPClient::disconnect()
{
	if (m_socket >= 0)
	{
		close(m_socket);
		m_socket = -1;
	}
}

TCPClient::~TCPClient()
{
	disconnect();
}

std::tuple<std::string, std::string> TCPClient::get_peer_ip_and_port() const
{
	sockaddr_storage sa{};
	socklen_t s = sizeof sa;
	// this var is for readability only
	auto *reinterpreted_sa = reinterpret_cast<sockaddr *>(&sa);

	getpeername(m_socket, reinterpreted_sa, &s);
	return { ::ntop(reinterpreted_sa), std::to_string(ntohs(::get_port(reinterpreted_sa))) };
}

std::string TCPClient::ntop(uint32_t ip)
{
	in_addr src = { ip };
	std::string ret;
	ret.resize(INET_ADDRSTRLEN);
	inet_ntop(AF_INET, &src, ret.data(), ret.size());
	return ret;
}
