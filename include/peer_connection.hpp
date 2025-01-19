#pragma once

#include "peer_message.hpp"
#include "socket.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <span>
#include <vector>

class PeerConnection {
	static constexpr size_t max_block_size = 16384;
	static constexpr size_t recv_buffer_size = max_block_size + 13;
	static constexpr int keepalive_timeout = 115; // in seconds

	enum class States {
		HANDSHAKE,
		LENGTH,
		MESSAGE,

		MAX_STATES,
	};

	Socket m_socket;

	States m_state = States::HANDSHAKE;

	std::vector<uint8_t> m_recv_buffer = std::vector<uint8_t>(recv_buffer_size);
	size_t m_recv_offset = 0;
	uint32_t m_message_length = 0;

	std::deque<std::unique_ptr<message::Message>> m_send_queue;
	size_t m_send_offset = 0;

	std::chrono::steady_clock::time_point m_tp = std::chrono::steady_clock::now();

	static constexpr size_t max_pending = 4;

	std::vector<message::Request> m_request_queue;

	void send_message(std::unique_ptr<message::Message> message);

	bool m_am_interested = false;
	bool m_peer_choking = true;

public:
	size_t m_rq_current = 0; // rq stands for request queue

	PeerConnection() = default;
	PeerConnection(const std::string &ip, const std::string &port,
		       const message::Handshake &handshake, const message::Bitfield &bitfield);

	message::Bitfield peer_bitfield;
	bool am_choking = true;
	bool peer_interested = false;

	void send_choke();
	void send_unchoke();
	void send_notinterested();
	void send_interested();

	// for piece like big piece, not message::Piece
	// the fact that these two things have the same name is so damn confusing
	void create_requests_for_piece(size_t index, size_t piece_size);
	// must be called repeatedly after each recv() of a message::Piece
	// if it returns not 0, it means that that message::Piece was the last part
	// of a big piece. Otherwise it returns 0
	int send_requests();
	void send_initial_requests();
	// during endgame should be called by all peers if piece was already downloaded
	// by someone else
	void cancel_requests_by_cancel(size_t index);
	void cancel_requests_on_choke();

	[[nodiscard]] bool is_downloading() const;

	[[nodiscard]] int send();
	[[nodiscard]] int recv();
	[[nodiscard]] int get_socket_fd() const;
	[[nodiscard]] bool should_wait_for_send() const;

	[[nodiscard]] std::span<const uint8_t> view_recv_message() const;
	[[nodiscard]] std::vector<uint8_t> get_recv_message();

	bool update_time();
};
