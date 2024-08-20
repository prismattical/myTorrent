#include "peer_connection.hpp"

#include "peer_message.hpp"
#include "socket.hpp"

#include <cstdint>
#include <cstring>
#include <netinet/in.h>
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

			m_state = States::LENGTH;
		}
		break;
	}

	case States::LENGTH: {
		m_socket.recv_nonblock(4, m_buffer, m_offset);
		if (m_offset == 4)
		{
			m_offset = 0;
			memcpy(&m_message_length, m_buffer.data(), sizeof m_message_length);
			m_message_length = ntohl(m_message_length);
			if (m_message_length != 0)
			{
				m_state = States::ID;
			} else
			{
				// TODO: parse KeepAlive message
			}
		}
		break;
	}

	case States::ID: {
		m_socket.recv_nonblock(1, m_buffer, m_offset);
		if (m_offset == 1)
		{
			m_offset = 0;
			const uint8_t id = m_buffer[0];
			switch (id)
			{
			case 0:
				m_am_choking = true;
				m_state = States::LENGTH;
				break;
			case 1:
				m_am_choking = false;
				m_state = States::LENGTH;
				break;
			case 2:
				m_peer_interested = true;
				m_state = States::LENGTH;
				break;
			case 3:
				m_peer_interested = false;
				m_state = States::LENGTH;
				break;
			case 4:
				m_state = States::HAVE;
				break;
			case 5:
				m_state = States::BITFIELD;
				break;
			case 6:
				m_state = States::REQUEST;
				break;
			case 7:
				m_state = States::PIECE;
				break;
			case 8:
				m_state = States::CANCEL;
				break;
			case 9:
				m_state = States::PORT;
				break;
			default:
				break;
			}
		}
		break;
	}

	case States::HAVE: {
		m_socket.recv_nonblock(4, m_buffer, m_offset);
		if (m_offset == 4)
		{
			m_offset = 0;

			uint32_t index;
			memcpy(&index, m_buffer.data(), sizeof index);
			index = ntohl(index);

			m_bitfield.set_index(index, true);

			// TODO: if new piece is needed, request it

			m_state = States::LENGTH;
		}
		break;
	}

	case States::BITFIELD: {
		m_socket.recv_nonblock(m_message_length - 1, m_buffer, m_offset);
		if (m_offset == m_message_length - 1)
		{
			m_offset = 0;
			m_bitfield = message::Bitfield(std::move(m_buffer));

			// TODO: if pieces from bitfield are needed, request them

			m_state = States::LENGTH;
		}
		break;
	}

	case States::REQUEST: {
		m_socket.recv_nonblock(12, m_buffer, m_offset);
		if (m_offset == 12)
		{
			m_offset = 0;

			// TODO: process request

			m_state = States::LENGTH;
		}
		break;
	}

	case States::PIECE: {
		m_socket.recv_nonblock(m_message_length - 1, m_buffer, m_offset);
		if (m_offset == m_message_length - 1)
		{
			m_offset = 0;

			// TODO: process piece and signal that it is downloaded

			m_state = States::LENGTH;
		}
		break;
	}

	case States::CANCEL: {
		m_socket.recv_nonblock(12, m_buffer, m_offset);
		if (m_offset == 12)
		{
			m_offset = 0;

			// TODO: process cancel

			m_state = States::LENGTH;
		}
		break;
	}

	case States::PORT: {
		m_socket.recv_nonblock(2, m_buffer, m_offset);
		if (m_offset == 2)
		{
			m_offset = 0;

			// TODO: process port

			m_state = States::LENGTH;
		}
		break;
	}

	default: {
		throw std::runtime_error("State value is MAX_STATES");
	}
	}
}
