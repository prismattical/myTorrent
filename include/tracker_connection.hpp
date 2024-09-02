#pragma once

#include "socket.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class TrackerConnection {
	enum class SendState {
		FORM_REQUEST,
		SEND_REQUEST,
		MAX_SEND_STATES
	};

	enum class RecvState {
	};

	Socket m_socket;

	std::string m_hostname;
	std::string m_port;
	std::string m_connection_id;

	std::vector<uint8_t> m_send_buffer;
	size_t m_send_offset = 0;
	SendState m_send_state = SendState::FORM_REQUEST;

	std::vector<uint8_t> m_recv_buffer = std::vector<uint8_t>(4096, 0);
	size_t m_recv_offset = 0;

public:
	TrackerConnection() = default;
	TrackerConnection(std::string hostname, std::string port, std::string connection_id);

	[[nodiscard]] bool get_socket_status() const;
	[[nodiscard]] int get_socket_fd() const;

	void connect();
	/**
	 * @return 0 if send was successful, 1 if send was partial
	 */
	int send_http(/*const std::string &trk_hostname, */ const std::string &trk_resource,
		      const std::string &info_hash, const std::string &upload_port);
	std::optional<std::string> recv_http();
};
