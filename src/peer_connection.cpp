#include "peer_connection.hpp"

#include "peer_message.hpp"
#include "socket.hpp"
#include <string>

PeerConnection::PeerConnection(const std::string &ip, const std::string &port,
			       const std::string &info_hash, const std::string &peer_id)
	: m_socket{ ip, port, Socket::TCP, Socket::IP_UNSPEC }
{
	message::Handshake hs(info_hash, peer_id);
	auto hs_ser = hs.serialized();
	m_socket.send_all(hs_ser);
	message::Handshake resp_hs(m_socket, info_hash);
}
