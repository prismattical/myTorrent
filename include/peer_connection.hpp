#pragma once

#include "socket.hpp"

class PeerConnection {
	Socket m_socket;

public:
	PeerConnection(const std::string &ip, const std::string &port);
};
