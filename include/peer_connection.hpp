#pragma once

#include "peer_message.hpp"
#include "socket.hpp"

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <variant>
#include <vector>

class PeerConnection {
	static constexpr int m_max_block_size = 16384;

	enum class States {
		HANDSHAKE,
		LENGTH,
		MESSAGE,

		MAX_STATES,
	};

	Socket m_socket;

	bool m_am_choking = true;
	bool m_peer_interested = false;

	bool m_peer_choking = true;
	bool m_am_interested = false;

	States m_state = States::HANDSHAKE;

	/*const*/ size_t m_default_recv_buffer_length;
	std::vector<uint8_t> m_recv_buffer;
	size_t m_recv_offset = 0;

	std::deque<std::unique_ptr<message::Message>> m_send_queue;
	std::unique_ptr<message::Message> m_current_send;
	std::span<const uint8_t> m_send_buffer;
	size_t m_send_offset = 0;

	uint32_t m_message_length;
	// view of memory owned by Download object
	/*const*/ std::span<const uint8_t> m_info_hash_binary;
	message::Bitfield m_peer_bitfield;

	void proceed_message();

public:
	PeerConnection() = default;
	PeerConnection(const std::string &ip, const std::string &port,
		       std::span<const uint8_t> info_hash_binary, std::span<const uint8_t> peer_id,
		       std::variant<std::reference_wrapper<const message::Bitfield>, size_t>
			       bitfield_or_length);

	int proceed_recv();
	int proceed_send();

	[[nodiscard]] bool get_socket_status() const;
	[[nodiscard]] int get_socket_fd() const;
};
