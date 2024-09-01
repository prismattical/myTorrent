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
	// message::Handshake hs(info_hash_binary, peer_id);
	// auto hs_ser = hs.serialized();
	// m_socket.send(hs_ser);
}

void PeerConnection::proceed()
{
	switch (m_state)
	{
	case States::RECV_HSLEN: {
		m_recv_buffer.resize(1);
		m_socket.recv(m_recv_buffer, m_recv_offset);
		if (m_recv_offset == 0)
		{
			const uint8_t len = m_recv_buffer[0];
			if (len != 19)
			{
				throw std::runtime_error(
					"Handshake: invalid protocol length received");
			}
			m_state = States::RECV_HSPRTCL;
		}
		break;
	}

	case States::RECV_HSPRTCL: {
		m_recv_buffer.resize(19);
		m_socket.recv(m_recv_buffer, m_recv_offset);
		if (m_recv_offset == 0)
		{
			if (memcmp(reinterpret_cast<char *>(m_recv_buffer.data()), "BitTorrent protocol",
				   19) != 0)
			{
				throw std::runtime_error(
					"Handshake: invalid protocol name received");
			}

			m_state = States::RECV_HSRSRVD;
		}
		break;
	}

	case States::RECV_HSRSRVD: {
		m_recv_buffer.resize(8);
		m_socket.recv(m_recv_buffer, m_recv_offset);
		if (m_recv_offset == 0)
		{
			// No extensions implemented so this part is just empty

			m_state = States::RECV_HSINFOHASH;
		}
		break;
	}

	case States::RECV_HSINFOHASH: {
		m_recv_buffer.resize(10);
		m_socket.recv(m_recv_buffer, m_recv_offset);
		if (m_recv_offset == 0)
		{
			if (memcmp(reinterpret_cast<char *>(m_recv_buffer.data()),
				   m_info_hash_binary.data(), 10) != 0)
			{
				throw std::runtime_error("Handshake: invalid info hash received");
			}
			m_state = States::RECV_HSPEERID;
		}
		break;
	}

	case States::RECV_HSPEERID: {
		m_recv_buffer.resize(10);
		m_socket.recv(m_recv_buffer, m_recv_offset);
		if (m_recv_offset == 0)
		{
			// additional check for peer id can be added

			m_state = States::RECV_LENGTH;
		}
		break;
	}

	case States::RECV_LENGTH: {
		m_recv_buffer.resize(4);
		m_socket.recv(m_recv_buffer, m_recv_offset);
		if (m_recv_offset == 0)
		{
			memcpy(&m_message_length, m_recv_buffer.data(), sizeof m_message_length);
			m_message_length = ntohl(m_message_length);
			if (m_message_length != 0)
			{
				m_state = States::RECV_ID;
			} else
			{
				// TODO: parse KeepAlive message
			}
		}
		break;
	}

	case States::RECV_ID: {
		m_recv_buffer.resize(1);
		m_socket.recv(m_recv_buffer, m_recv_offset);
		if (m_recv_offset == 0)
		{
			const uint8_t id = m_recv_buffer[0];
			switch (id)
			{
			case 0:
				m_am_choking = true;
				m_state = States::RECV_LENGTH;
				break;
			case 1:
				m_am_choking = false;
				m_state = States::RECV_LENGTH;
				break;
			case 2:
				m_peer_interested = true;
				m_state = States::RECV_LENGTH;
				break;
			case 3:
				m_peer_interested = false;
				m_state = States::RECV_LENGTH;
				break;
			case 4:
				m_state = States::RECV_HAVE;
				break;
			case 5:
				m_state = States::RECV_BITFIELD;
				break;
			case 6:
				m_state = States::RECV_REQUEST;
				break;
			case 7:
				m_state = States::RECV_PIECE;
				break;
			case 8:
				m_state = States::RECV_CANCEL;
				break;
			case 9:
				m_state = States::RECV_PORT;
				break;
			default:
				break;
			}
		}
		break;
	}

	case States::RECV_HAVE: {
		m_recv_buffer.resize(4);
		m_socket.recv(m_recv_buffer, m_recv_offset);
		if (m_recv_offset == 0)
		{
			uint32_t index;
			memcpy(&index, m_recv_buffer.data(), sizeof index);
			index = ntohl(index);

			m_bitfield.set_index(index, true);

			// TODO: if new piece is needed, request it

			m_state = States::RECV_LENGTH;
		}
		break;
	}

	case States::RECV_BITFIELD: {
		m_recv_buffer.resize(m_message_length - 1);
		m_socket.recv(m_recv_buffer, m_recv_offset);
		if (m_recv_offset == 0)
		{
			m_bitfield = message::Bitfield(std::move(m_recv_buffer));

			// TODO: if pieces from bitfield are needed, request them

			m_state = States::RECV_LENGTH;
		}
		break;
	}

	case States::RECV_REQUEST: {
		m_recv_buffer.resize(12);
		m_socket.recv(m_recv_buffer, m_recv_offset);
		if (m_recv_offset == 0)
		{
			// TODO: process request

			m_state = States::RECV_LENGTH;
		}
		break;
	}

	case States::RECV_PIECE: {
		m_recv_buffer.resize(m_message_length - 1);
		m_socket.recv(m_recv_buffer, m_recv_offset);
		if (m_recv_offset == 0)
		{
			// TODO: process piece and signal that it is downloaded

			m_state = States::RECV_LENGTH;
		}
		break;
	}

	case States::RECV_CANCEL: {
		m_recv_buffer.resize(12);
		m_socket.recv(m_recv_buffer, m_recv_offset);
		if (m_recv_offset == 0)
		{
			// TODO: process cancel

			m_state = States::RECV_LENGTH;
		}
		break;
	}

	case States::RECV_PORT: {
		m_recv_buffer.resize(2);
		m_socket.recv(m_recv_buffer, m_recv_offset);
		if (m_recv_offset == 0)
		{
			// TODO: process port

			m_state = States::RECV_LENGTH;
		}
		break;
	}

	default: {
		throw std::runtime_error("State value is MAX_STATES");
	}
	}
}
