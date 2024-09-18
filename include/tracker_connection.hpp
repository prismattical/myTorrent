#pragma once

#include "socket.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class TrackerConnection {
	static constexpr int m_recv_buffer_length = 4096;

	enum class SendState {
		FORM_REQUEST,
		SEND_REQUEST,
		MAX_SEND_STATES
	};

	Socket m_socket;

	std::string m_hostname;
	std::string m_port;
	std::string m_connection_id;

	std::vector<uint8_t> m_send_buffer;
	size_t m_send_offset = 0;
	SendState m_send_state = SendState::FORM_REQUEST;

	std::array<uint8_t, m_recv_buffer_length> m_recv_buffer;
	size_t m_recv_offset = 0;

public:
	TrackerConnection() = default;
	TrackerConnection(std::string hostname, std::string port, std::string connection_id);

	[[nodiscard]] bool get_socket_status() const;
	[[nodiscard]] int get_socket_fd() const;

	void connect();
	// returns 0 if send was successful, 1 if send was partial
	int send_http(const std::string &trk_resource, const std::string &info_hash,
		      const std::string &upload_port);
	std::optional<std::string> recv_http();
};
