#pragma once

#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

// RAII wrapper for non blocking Linux sockets
class Socket {
public:
	enum Protocol {
		TCP,
		UDP, // not implemented
		MAX_PROTOCOLS,
	};
	enum IPVersion {
		IPV4,
		IPV6,
		IP_UNSPEC,
		MAX_IP_VERSIONS,
	};

	class ConnectionResetException : public std::runtime_error {
	public:
		ConnectionResetException(const std::string &prefix = "");
	};

private:
	int m_socket = -1;

	mutable bool m_connected = false;

public:
	Socket(const std::string &hostname, const std::string &port, Protocol protocol,
	       IPVersion ip_version);

	Socket() = default;

	Socket(const Socket &other) = delete;
	Socket &operator=(const Socket &other) = delete;

	Socket(Socket &&other) noexcept;
	Socket &operator=(Socket &&other) noexcept;

	void validate_connect() const;

	void send(std::span<const uint8_t> buffer, size_t &offset) const;
	void recv(std::span<uint8_t> buffer, size_t &offset) const;

	[[nodiscard]] bool connected() const;
	[[nodiscard]] int get_fd() const;

	[[nodiscard]] std::tuple<std::string, std::string> get_peer_ip_and_port() const;

	static std::string ntop(uint32_t ip);

	~Socket();
};
