#pragma once

#include "peer_message.hpp"
#include "socket.hpp"

#include <cstdint>
#include <vector>

class PeerConnection {
	enum class States {
		NOT_CONNECTED,

		SEND_HS,
		SEND_KEEPALIVE,
		SEND_CHOKE,
		SEND_UNCHOKE,
		SEND_INTERESTED,
		SEND_NOTINTERESTED,
		SEND_HAVE,
		SEND_BITFIELD,
		SEND_REQUEST,
		SEND_PIECE,
		SEND_CANCEL,
		SEND_PORT,
		// handshake
		RECV_HSLEN,
		RECV_HSPRTCL,
		RECV_HSRSRVD,
		RECV_HSINFOHASH,
		RECV_HSPEERID,
		// common
		RECV_LENGTH,
		RECV_ID,
		// specific
		RECV_HAVE,
		RECV_BITFIELD,
		RECV_REQUEST,
		RECV_PIECE,
		RECV_CANCEL,
		RECV_PORT,

		MAX_STATES,
	};

	Socket m_socket;

	bool m_am_choking = true;
	bool m_peer_interested = false;

	bool m_peer_choking = true;
	bool m_am_interested = false;

	States m_state = States::RECV_HSLEN;

	// TODO: decide on buffer reserve value
	std::vector<uint8_t> m_recv_buffer = std::vector<uint8_t>(4096, 0);
	size_t m_recv_offset = 0;

	std::vector<uint8_t> m_send_buffer;
	size_t m_send_offset = 0;

	uint32_t m_message_length;
	std::string m_info_hash_binary;
	message::Bitfield m_bitfield;
	message::Request m_current_request;

public:
	PeerConnection() = default;
	PeerConnection(const std::string &ip, const std::string &port,
		       const std::string &info_hash_binary, const std::string &peer_id);

	void proceed();
};
