#pragma once

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

// RAII wrapper for Linux sockets
class Socket {
public:
	enum Protocol {
		TCP,
		UDP,
		MAX_PROTOCOLS,
	};
	enum IPVersion {
		IPV4,
		IPV6,
		IP_UNSPEC,
		MAX_IP_VERSIONS,
	};

private:
	int m_socket;

public:
	Socket(const std::string &hostname, const std::string &port, Protocol protocol,
	       IPVersion ip_version);

	Socket();

	Socket(const Socket &other) = delete;
	Socket &operator=(const Socket &other) = delete;

	Socket(Socket &&other) noexcept;
	Socket &operator=(Socket &&other) noexcept;

	void send_all(const std::vector<unsigned char> &data) const;
	[[nodiscard]] std::vector<unsigned char> recv_until_close() const;
	[[nodiscard]] std::vector<unsigned char> recv_some(size_t len) const;
	[[nodiscard]] uint32_t recv_length() const;
	[[nodiscard]] std::tuple<std::string, std::string> get_peer_ip_and_port() const;
	void recv_nonblock(size_t len, std::vector<uint8_t> &buffer, size_t &offset) const;

	static std::string ntop(uint32_t ip);

	~Socket();
};
