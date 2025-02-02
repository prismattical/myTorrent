#pragma once

#include "socket.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

class TrackerConnection {
	static constexpr int recv_buffer_size = 4096;

	TCPClient m_socket;

	std::vector<uint8_t> m_send_buffer;
	size_t m_send_offset = 0;

	std::vector<uint8_t> m_recv_buffer = std::vector<uint8_t>(recv_buffer_size);
	size_t m_recv_offset = 0;

	bool m_request_sent = false;

	std::chrono::steady_clock::time_point m_tp;
	long long m_timeout = 0;

public:
	TrackerConnection() = default;
	TrackerConnection(const std::string &hostname, const std::string &port,
			  std::span<const uint8_t> connection_id, const std::string &info_hash);

	void connect(const std::string &hostname, const std::string &port,
		     std::span<const uint8_t> connection_id, const std::string &info_hash);
	void disconnect();

	[[nodiscard]] int get_socket_fd() const;
	[[nodiscard]] bool should_wait_for_send() const;
	[[nodiscard]] std::span<const uint8_t> view_recv_message() const;
	[[nodiscard]] std::vector<uint8_t> &&get_recv_message();

	[[nodiscard]] int send();
	[[nodiscard]] int recv();

	void set_timeout(long long seconds);
	[[nodiscard]] bool update_time();
};
