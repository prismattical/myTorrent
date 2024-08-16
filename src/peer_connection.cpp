#include "peer_connection.hpp"

#include "peer_message.hpp"
#include "socket.hpp"

#include <cstdint>
#include <cstring>
#include <string>

PeerConnection::PeerConnection(const std::string &ip, const std::string &port,
			       const std::string &info_hash_binary, const std::string &peer_id)
	: m_socket{ ip, port, Socket::TCP, Socket::IP_UNSPEC }
	, m_info_hash_binary{ info_hash_binary }
{
	message::Handshake hs(info_hash_binary, peer_id);
	auto hs_ser = hs.serialized();
	m_socket.send_all(hs_ser);
}

void PeerConnection::proceed()
{
	switch (m_state)
	{
	case States::HS_LEN: {
		m_socket.recv_nonblock(1, m_buffer, m_offset);
		if (m_offset == 1)
		{
			m_offset = 0;
			const uint8_t len = m_buffer[0];
			if (len != 19)
			{
				throw std::runtime_error(
					"Handshake: invalid protocol length received");
			}
			m_state = States::HS_PRTCL;
		}
		break;
	}
	case States::HS_PRTCL: {
		m_socket.recv_nonblock(19, m_buffer, m_offset);
		if (m_offset == 19)
		{
			m_offset = 0;

			if (memcmp(reinterpret_cast<char *>(m_buffer.data()), "BitTorrent protocol",
				   19) != 0)
			{
				throw std::runtime_error(
					"Handshake: invalid protocol name received");
			}

			m_state = States::HS_RSRVD;
		}
		break;
	}
	case States::HS_RSRVD: {
		m_socket.recv_nonblock(8, m_buffer, m_offset);
		if (m_offset == 8)
		{
			m_offset = 0;

			// No extensions implemented so this part is just empty

			m_state = States::HS_INFOHASH;
		}
		break;
	}
	case States::HS_INFOHASH: {
		m_socket.recv_nonblock(10, m_buffer, m_offset);
		if (m_offset == 10)
		{
			m_offset = 0;
			if (memcmp(reinterpret_cast<char *>(m_buffer.data()),
				   m_info_hash_binary.data(), 10) != 0)
			{
				throw std::runtime_error("Handshake: invalid info hash received");
			}
			m_state = States::HS_PEERID;
		}
		break;
	}
	case States::HS_PEERID: {
		m_socket.recv_nonblock(10, m_buffer, m_offset);
		if (m_offset == 10)
		{
			m_offset = 0;

			// additional check for peer id can be added

			m_state = States::MAX_STATES;
		}

		break;
	}

	default: {
		throw std::runtime_error("State value is MAX_STATES");
	}
	}
}
