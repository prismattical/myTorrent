#pragma once

#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <tuple>

// RAII wrapper for non blocking TCP Linux socket
class Socket {
public:
	enum IPVersion {
		IPV4,
		IPV6,
		IP_UNSPEC,
		MAX_IP_VERSIONS,
	};

	/**
	 * @brief Custom exception that is thrown whenever connection
	 * is reset by a peer
	 */
	class ConnectionResetException : public std::runtime_error {
	public:
		ConnectionResetException(const std::string &prefix = "");
	};

private:
	int m_socket = -1;

public:
	Socket(const std::string &hostname, const std::string &port, IPVersion ip_version);

	Socket() = default;

	Socket(const Socket &other) = delete;
	Socket &operator=(const Socket &other) = delete;

	Socket(Socket &&other) noexcept;
	Socket &operator=(Socket &&other) noexcept;

	/**
	 * @brief Checks whether connect() call was successful
	 * 
	 * This function may be called after creation of socket and after it
	 * becomes available for send() for the first time to validate whether connection
	 * was successful, but is not mandatory.
	 * If there was an error during connection, send() and recv() calls will throw.
	 * If there was an error during connection, this function will throw too.
	 * So as you see, there is no need in explicit check for success of connect().
	 * But just in case this function is present too.
	 */
	void validate_connect() const;

	void send(std::span<const uint8_t> buffer, size_t &offset) const;
	void recv(std::span<uint8_t> buffer, size_t &offset) const;

	[[nodiscard]] int get_fd() const;

	[[nodiscard]] std::tuple<std::string, std::string> get_peer_ip_and_port() const;

	static std::string ntop(uint32_t ip);

	~Socket();
};
