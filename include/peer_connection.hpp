#pragma once

#include "peer_message.hpp"
#include "socket.hpp"

class PeerConnection {
	Socket m_socket;

	bool m_am_choking = true;
	bool m_am_interested = false;
	bool m_peer_choking = true;
	bool m_peer_interested = false;

	message::Bitfield m_bitfield;

public:
	PeerConnection(const std::string &ip, const std::string &port, const std::string &info_hash,
		       const std::string &peer_id);
};
