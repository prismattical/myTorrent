#include "peer_connection.hpp"

#include "peer_message.hpp"
#include "socket.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

bool PeerConnection::get_socket_status() const
{
	return m_socket.connected();
}

int PeerConnection::get_socket_fd() const
{
	return m_socket.get_fd();
}

void PeerConnection::proceed_message()
{
	switch (m_recv_buffer[4] /*id of the message*/)
	{
	case 0:
		// no need to construct this message
		m_am_choking = true;
		// should be return with notification after choking happens
		break;
	case 1:
		// no need to construct this message
		m_am_choking = false;
		// should be return with notification after unchoking happens
		break;
	case 2:
		// no need to construct this message
		m_peer_interested = true;
		break;
	case 3:
		// no need to construct this message
		m_peer_interested = false;
		break;
	case 4: {
		auto have = message::Have({ m_recv_buffer.data(), 9 });
		m_peer_bitfield.set_index(have.get_index(), true);
		// should notify that new pieces may be available
		break;
	}
	case 5: {
		// TODO fix bug
		m_peer_bitfield = message::Bitfield(std::move(m_recv_buffer));
		m_recv_buffer.resize(m_default_recv_buffer_length);
		// should notify that new pieces may be available
		break;
	}
	case 6: {
		auto request = message::Request({ m_recv_buffer.data(), 17 });
		// should notify that piece was requested
		break;
	}
	case 7: {
		auto piece = message::Piece(std::move(m_recv_buffer));
		m_recv_buffer.resize(m_default_recv_buffer_length);
		// should notify and move away the piece
		break;
	}
	case 8: {
		auto cancel = message::Cancel({ m_recv_buffer.data(), 17 });
		break;
	}
	case 9:
		// do nothing because not implemented
		break;
	default:
		throw std::runtime_error("Invalid message id value");
	}
}

PeerConnection::PeerConnection(
	const std::string &ip, const std::string &port, std::span<const uint8_t> info_hash_binary,
	std::span<const uint8_t> peer_id,
	std::variant<std::reference_wrapper<const message::Bitfield>, size_t> bitfield_or_length)
	: m_socket{ ip, port, Socket::TCP, Socket::IP_UNSPEC }
	, m_info_hash_binary{ info_hash_binary }
{
	m_send_queue.emplace_back(std::make_unique<message::Handshake>(info_hash_binary, peer_id));

	using message::Bitfield;
	if (std::holds_alternative<std::reference_wrapper<const Bitfield>>(bitfield_or_length))
	{
		const Bitfield &bf =
			std::get<std::reference_wrapper<const Bitfield>>(bitfield_or_length).get();

		m_send_queue.emplace_back(std::make_unique<Bitfield>(bf));

		m_default_recv_buffer_length = bf.get_container_size();
	} else
	{
		m_default_recv_buffer_length = std::get<size_t>(bitfield_or_length);
	}

	// 68 is the size of the handshake message
	m_default_recv_buffer_length = std::max<size_t>(m_default_recv_buffer_length, 68);
}

int PeerConnection::proceed_recv()
{
	switch (m_state)
	{
	case States::HANDSHAKE:
		m_socket.recv({ m_recv_buffer.data(), 68 }, m_recv_offset);
		if (m_recv_offset == 0)
		{
			message::Handshake hs({ m_recv_buffer.data(), 68 });
			if (hs.get_pstrlen()[0] != 19 ||
			    memcmp(hs.get_pstr().data(), "BitTorrent protocol", 19) != 0 ||
			    memcmp(hs.get_info_hash().data(), m_info_hash_binary.data(), 20) != 0)
			{
				throw std::runtime_error("Invalid peer handshake");
			}
			m_state = States::LENGTH;
		}
		break;
	case States::LENGTH:
		m_socket.recv({ m_recv_buffer.data(), 4 }, m_recv_offset);
		if (m_recv_offset == 0)
		{
			memcpy(&m_message_length, m_recv_buffer.data(), sizeof(m_message_length));
			m_message_length = ntohl(m_message_length);
			if (m_message_length == 0)
			{
				// proceed KeepAlive message
				break;
			}
			if (m_message_length > m_default_recv_buffer_length)
			{
				m_default_recv_buffer_length = m_message_length;
				m_recv_buffer.resize(m_default_recv_buffer_length);
			}
			m_state = States::MESSAGE;
		}
		[[fallthrough]]; // this can save a spare call to poll()
	case States::MESSAGE:
		m_socket.recv({ m_recv_buffer.data() + 4, m_message_length }, m_recv_offset);
		if (m_recv_offset == 0)
		{
			proceed_message();
			m_state = States::LENGTH;
		}
		break;
	default:
		throw std::runtime_error("Invalid state value");
	}
}

int PeerConnection::proceed_send()
{
	if (m_send_offset == 0)
	{
		if (!m_send_queue.empty())
		{
			m_current_send.swap(m_send_queue.front());
			m_send_queue.pop_front();
			m_send_buffer = m_current_send->serialized();
		} else
		{
			return 0;
		}
	}
	m_socket.send(m_send_buffer, m_send_offset);
	return 1;
}
