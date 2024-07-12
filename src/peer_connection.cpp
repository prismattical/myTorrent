#include "peer_connection.hpp"

#include "socket.hpp"

PeerConnection::PeerConnection(const std::string &ip, const std::string &port)
	: m_socket{ ip, port, Socket::TCP, Socket::IP_UNSPEC }
{
}
