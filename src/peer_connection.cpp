#include "peer_connection.hpp"

#include "peer_message.hpp"
#include "socket.hpp"

#include <arpa/inet.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <netinet/in.h>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

int PeerConnection::get_socket_fd() const
{
	return m_socket.get_fd();
}

PeerConnection::PeerConnection(const std::string &ip, const std::string &port,
			       const message::Handshake &handshake,
			       const message::Bitfield &bitfield)
{
	connect(ip, port, handshake, bitfield);
}

void PeerConnection::connect(const std::string &ip, const std::string &port,
			     const message::Handshake &handshake, const message::Bitfield &bitfield)
{
	m_socket.connect(ip, port);

	peer_bitfield = message::Bitfield(bitfield.get_bitfield_length());
	m_send_queue.emplace_back(std::make_unique<message::Handshake>(handshake));
	m_send_queue.emplace_back(std::make_unique<message::Bitfield>(bitfield));
}

void PeerConnection::disconnect()
{
	m_socket.disconnect();
}

int PeerConnection::recv()
{
	static constexpr size_t hs_len = 68;
	static constexpr size_t length_len = 4;

	switch (m_state)
	{
	case States::HANDSHAKE: {
		long rc = m_socket.recv2(
			{ m_recv_buffer.data() + m_recv_offset, hs_len - m_recv_offset });

		if (rc == -1)
		{
			return 1;
		}

		m_recv_offset += rc;

		if (m_recv_offset == hs_len - 1)
		{
			m_recv_offset = 0;
			m_state = States::LENGTH;
			// setting it like this allows to simplify implementation of view_recv_message()
			m_message_length = hs_len - length_len;
			return 0;
		}
		return 1;
	}
	case States::LENGTH: {
		long rc = m_socket.recv2(
			{ m_recv_buffer.data() + m_recv_offset, length_len - m_recv_offset });
		if (rc == -1)
		{
			return 1;
		}

		m_recv_offset += rc;

		if (m_recv_offset != length_len - 1)
		{
			return 1;
		}

		m_recv_offset = 0;
		memcpy(&m_message_length, m_recv_buffer.data(), sizeof(m_message_length));
		m_message_length = ntohl(m_message_length);
		if (m_message_length == 0)
		{
			// KeepAlive message received
			return 0;
		}
		m_state = States::MESSAGE;
		// this can save a spare call to poll()
		[[fallthrough]];
	}
	case States::MESSAGE: {
		long rc = m_socket.recv2({ m_recv_buffer.data() + length_len + m_recv_offset,
					   m_message_length - m_recv_offset });

		if (rc == -1)
		{
			return 1;
		}

		m_recv_offset += rc;

		if (m_recv_offset == m_message_length - 1)
		{
			m_recv_offset = 0;
			m_state = States::LENGTH;
			return 0;
		}
		return 1;
	}
	default:
		throw std::runtime_error("Invalid state value");
	}
	return 1;
}

int PeerConnection::send()
{
	long rc = m_socket.send({ m_send_queue.front()->serialized().data() + m_send_offset,
				  m_send_queue.front()->serialized().size() - m_send_offset });

	if (rc == -1)
	{
		return 1;
	}

	m_send_offset += rc;

	if (m_send_offset == m_send_queue.front()->serialized().size() - 1)
	{
		m_send_queue.pop_front();
		return 0;
	}
	return 1;
}

bool PeerConnection::should_wait_for_send() const
{
	return !m_send_queue.empty();
}

std::span<const uint8_t> PeerConnection::view_recv_message() const
{
	return { m_recv_buffer.data(), 4 + m_message_length };
}

std::vector<uint8_t> &&PeerConnection::get_recv_message()
{
	auto &&ret = std::move(m_recv_buffer);
	m_recv_buffer = std::vector<uint8_t>(recv_buffer_size);
	return std::move(ret);
}

bool PeerConnection::update_time()
{
	using std::chrono::steady_clock;
	using std::chrono::seconds;

	auto curr_tp = steady_clock::now();
	if (duration_cast<seconds>(m_tp - curr_tp).count() > keepalive_timeout)
	{
		m_send_queue.push_back(std::make_unique<message::KeepAlive>());
		m_tp = curr_tp;
		return true;
	}
	return false;
}

void PeerConnection::send_message(std::unique_ptr<message::Message> message)
{
	m_send_queue.emplace_back(std::move(message));
}

void PeerConnection::create_requests_for_piece(size_t index, size_t piece_size)
{
	m_rq_current = 0;

	m_request_queue.resize((piece_size + max_block_size - 1) / max_block_size);

	for (size_t i = 0; i < m_request_queue.size(); ++i)
	{
		uint32_t begin = i * max_block_size;
		m_request_queue[i] = { static_cast<uint32_t>(index), begin,
				       std::min<uint32_t>(piece_size - begin, max_block_size) };
	}
}
void PeerConnection::send_initial_requests()
{
	for (size_t i = 0; i < max_pending && i < m_request_queue.size(); ++i)
	{
		m_send_queue.emplace_back(
			std::make_unique<message::Request>(m_request_queue.at(i)));
	}
}

int PeerConnection::send_requests()
{
	if (m_rq_current + max_pending < m_request_queue.size())
	{
		m_send_queue.emplace_back(std::make_unique<message::Request>(
			m_request_queue.at(m_rq_current + max_pending)));
	}

	++m_rq_current;

	if (m_rq_current == m_request_queue.size())
	{
		return 1;
	}

	return 0;
}

void PeerConnection::cancel_requests_by_cancel(size_t index)
{
	if (!m_request_queue.empty() && index == m_request_queue[0].get_index())
	{
		for (size_t i = m_rq_current;
		     i < m_rq_current + max_pending - 1 && i < m_request_queue.size(); ++i)
		{
			m_send_queue.emplace_back(std::make_unique<message::Cancel>(
				m_request_queue[i].create_cancel()));
		}
		m_request_queue.clear();
	}
}

void PeerConnection::cancel_requests_on_choke()
{
	m_request_queue.clear();
}

bool PeerConnection::is_downloading() const
{
	return !m_request_queue.empty();
}

void PeerConnection::send_choke()
{
	if (!m_peer_choking)
	{
		send_message(std::make_unique<message::Choke>());
		m_peer_choking = true;
	}
}

void PeerConnection::send_unchoke()
{
	if (m_peer_choking)
	{
		send_message(std::make_unique<message::Unchoke>());
		m_peer_choking = false;
	}
}

void PeerConnection::send_interested()
{
	if (!m_am_interested)
	{
		send_message(std::make_unique<message::Interested>());
		m_am_interested = true;
	}
}

void PeerConnection::send_notinterested()
{
	if (m_am_interested)
	{
		send_message(std::make_unique<message::NotInterested>());
		m_am_interested = false;
	}
}
