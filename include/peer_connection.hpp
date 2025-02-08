#pragma once

#include "peer_message.hpp"
#include "piece.hpp"
#include "socket.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <vector>

class RequestQueue {
public:
	static constexpr size_t max_block_size = 16384;
	static constexpr std::size_t max_pending = 4;

private:
	std::deque<message::Request> m_requests;
	std::size_t m_current_req = 0;
	std::size_t m_forward_req = 0;

public:
	RequestQueue() = default;
	void reset();
	void create_requests_for_piece(size_t index, size_t size);
	[[nodiscard]] int send_request(class PeerConnection *parent);
	[[nodiscard]] int validate_block(const message::Piece &block);
	[[nodiscard]] std::set<std::size_t> assigned_pieces() const;
	[[nodiscard]] bool empty() const;
};

class PeerConnection {
	friend RequestQueue;

public:
	static constexpr size_t max_block_size = RequestQueue::max_block_size;
	static constexpr size_t recv_buffer_size = 4 + 1 + 4 + 4 + max_block_size;
	static constexpr int keepalive_timeout = 115; // in seconds

private:
	enum class States {
		HANDSHAKE,
		LENGTH,
		MESSAGE,

		MAX_STATES,
	};

	TCPClient m_socket;

	States m_state = States::HANDSHAKE;

	std::vector<uint8_t> m_recv_buffer = std::vector<uint8_t>(recv_buffer_size);
	size_t m_recv_offset = 0;
	uint32_t m_message_length = 0;

	std::deque<std::unique_ptr<message::Message>> m_send_queue;
	size_t m_send_offset = 0;

	std::chrono::steady_clock::time_point m_tp = std::chrono::steady_clock::now();

	RequestQueue m_request_queue;
	static constexpr size_t m_allowed_failures = 4;
	size_t m_failures = 0;
	ReceivedPiece m_assigned_piece;

	bool m_am_interested = false;
	bool m_peer_choking = true;

	void add_message_to_queue(std::unique_ptr<message::Message> message);

public:
	message::Bitfield peer_bitfield;
	bool am_choking = true;
	bool peer_interested = false;

	PeerConnection() = default;
	PeerConnection(const std::string &ip, const std::string &port,
		       const message::Handshake &handshake, const message::Bitfield &bitfield);

	void connect(const std::string &ip, const std::string &port,
		     const message::Handshake &handshake, const message::Bitfield &bitfield);
	void disconnect();

	void send_keepalive();
	void send_choke();
	void send_unchoke();
	void send_notinterested();
	void send_interested();

	/**
	 * @brief Sends requests
	 * 
	 * @return 0 on success
	 * @return 1 if caller should ask dl_strategy to assign next piece to this peer
	 * before calling again
	 */
	[[nodiscard]] int send_request();
	/**
	 * @brief Creates requests for a piece 
	 * that later are sent with send_request() method
	 * 
	 * @param index The index of the piece
	 * @param size The size of the piece
	 */
	void create_requests_for_piece(size_t index, size_t size);
	/**
	 * @brief Validates and adds the block to an assigned piece
	 * 
	 * @return -1 on failure
	 * @return 0 on sucess
	 * @return 1 on sucess and that block was the last block of the piece
	 */
	[[nodiscard]] int add_block();

	/**
	 * @brief Resets request queue
	 */
	void reset_request_queue();
	/**
	 * @brief Returns all the pieces that were assigned by dl strategy
	 * 
	 * @return A set of all unique indices of pieces that were assigned
	 */
	[[nodiscard]] std::set<std::size_t> assigned_pieces() const;

	[[nodiscard]] bool is_downloading() const;

	[[nodiscard]] int send();
	[[nodiscard]] int recv();
	[[nodiscard]] int get_socket_fd() const;
	[[nodiscard]] bool should_wait_for_send() const;

	[[nodiscard]] ReceivedPiece &&get_received_piece();

	[[nodiscard]] std::span<const uint8_t> view_recv_message() const;

	bool update_time();
};
