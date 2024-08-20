#pragma once

#include "peer_message.hpp"
#include "socket.hpp"

#include <cstdint>
#include <vector>

class PeerConnection {
	enum class States {
		// handshake
		HS_LEN,
		HS_PRTCL,
		HS_RSRVD,
		HS_INFOHASH,
		HS_PEERID,

		LENGTH,
		ID,

		HAVE,
		BITFIELD,
		REQUEST,
		PIECE,
		CANCEL,
		PORT,

		MAX_STATES,
	};

	Socket m_socket;

	bool m_am_choking = true;
	bool m_peer_interested = false;
	
	bool m_peer_choking = true;
	bool m_am_interested = false;

	States m_state = States::HS_LEN;
	std::vector<uint8_t> m_buffer;
	size_t m_offset = 0;

	uint32_t m_message_length;

	std::string m_info_hash_binary;
	message::Bitfield m_bitfield;
	message::Request m_current_request;

public:
	PeerConnection(const std::string &ip, const std::string &port,
		       const std::string &info_hash_binary, const std::string &peer_id);

	void proceed();
};
