#include "peer_connection.hpp"

#include "peer_message.hpp"
#include "socket.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cassert>
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

void RequestQueue::reset()
{
	m_requests.clear();
	m_current_req = 0;
	m_forward_req = 0;
}

void RequestQueue::create_requests_for_piece(size_t index, size_t size)
{
	// ceiling rounding division
	const std::size_t blocks = (size + max_block_size - 1) / max_block_size;

	for (size_t i = 0; i < blocks; ++i)
	{
		uint32_t begin = i * max_block_size;
		m_requests.emplace_back(static_cast<uint32_t>(index), begin,
					std::min<uint32_t>(size - begin, max_block_size));
	}
}

int RequestQueue::send_request(PeerConnection *parent)
{
	while (m_forward_req < m_current_req + max_pending && m_forward_req < m_requests.size())
	{
		parent->add_message_to_queue(
			std::make_unique<message::Request>(m_requests[m_forward_req]));
		++m_forward_req;
	}
	if (m_forward_req >= m_requests.size())
	{
		// add next piece to the queue
		return 1;
	}

	return 0;
}

int RequestQueue::validate_block(const message::Piece &block)
{
	const message::Request &rq = m_requests[m_current_req];
	const bool res = rq.get_index() == block.get_index() &&
			 rq.get_begin() == block.get_begin() &&
			 rq.get_length() == block.get_length();
	if (!res)
	{
		// block is invalid
		return -1;
	}

	++m_current_req;
	if (m_current_req == m_requests.size() || // final block of final piece
	    m_requests[m_current_req].get_index() != block.get_index())
	{
		const size_t ind = block.get_index();

		const size_t erased = std::erase_if(m_requests, [ind](const message::Request &rq) {
			return rq.get_index() == ind;
		});

		m_current_req -= erased;
		m_forward_req -= erased;

		assert(m_current_req == 0 && "It should be zero though");

		// block is valid and last block of the piece
		return 1;
	}
	// block is valid
	return 0;
}

std::set<std::size_t> RequestQueue::assigned_pieces() const
{
	std::set<std::size_t> ret;
	for (const auto &rq : m_requests)
	{
		ret.insert(rq.get_index());
	}
	return ret;
}

bool RequestQueue::empty() const
{
	return m_requests.empty();
}

int PeerConnection::get_socket_fd() const
{
	return m_socket.get_fd();
}

ReceivedPiece &&PeerConnection::get_received_piece()
{
	return std::move(m_assigned_piece);
}

void PeerConnection::add_message_to_queue(std::unique_ptr<message::Message> message)
{
	m_send_queue.emplace_back(std::move(message));
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
	add_message_to_queue(std::make_unique<message::Handshake>(handshake));
	add_message_to_queue(std::make_unique<message::Bitfield>(bitfield));
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
	std::span<const std::uint8_t> curr_mes = m_send_queue.front()->serialized();
	long rc =
		m_socket.send({ curr_mes.data() + m_send_offset, curr_mes.size() - m_send_offset });

	if (rc == -1)
	{
		return 1;
	}

	m_send_offset += rc;

	if (m_send_offset == curr_mes.size() - 1)
	{
		m_send_queue.pop_front();
		if (m_request_queue.empty())
		{
			return 0;
		}
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

bool PeerConnection::update_time()
{
	using std::chrono::steady_clock;
	using std::chrono::seconds;

	auto curr_tp = steady_clock::now();
	if (duration_cast<seconds>(m_tp - curr_tp).count() > keepalive_timeout)
	{
		send_keepalive();
		m_tp = curr_tp;
		return true;
	}
	return false;
}

int PeerConnection::send_request()
{
	return m_request_queue.send_request(this);
}

void PeerConnection::create_requests_for_piece(size_t index, size_t size)
{
	m_request_queue.create_requests_for_piece(index, size);
}

int PeerConnection::add_block()
{
	message::Piece block(std::move(m_recv_buffer));
	m_recv_buffer.resize(recv_buffer_size);

	int rc = m_request_queue.validate_block(block);
	if (rc != -1)
	{
		m_assigned_piece.add_block(std::move(block));
		m_failures = 0;
		return rc;
	}
	if (rc == -1)
	{
		if (++m_failures >= m_allowed_failures)
		{
			return -1;
		}
		return 0;
	}
	// must never be there
	return -1;
}

std::set<std::size_t> PeerConnection::assigned_pieces() const
{
	return m_request_queue.assigned_pieces();
}

void PeerConnection::reset_request_queue()
{
	m_request_queue.reset();
}

bool PeerConnection::is_downloading() const
{
	return !m_request_queue.empty();
}

void PeerConnection::send_keepalive()
{
	add_message_to_queue(std::make_unique<message::KeepAlive>());
}

void PeerConnection::send_choke()
{
	if (!m_peer_choking)
	{
		add_message_to_queue(std::make_unique<message::Choke>());
		m_peer_choking = true;
	}
}

void PeerConnection::send_unchoke()
{
	if (m_peer_choking)
	{
		add_message_to_queue(std::make_unique<message::Unchoke>());
		m_peer_choking = false;
	}
}

void PeerConnection::send_interested()
{
	if (!m_am_interested)
	{
		add_message_to_queue(std::make_unique<message::Interested>());
		m_am_interested = true;
	}
}

void PeerConnection::send_notinterested()
{
	if (m_am_interested)
	{
		add_message_to_queue(std::make_unique<message::NotInterested>());
		m_am_interested = false;
	}
}
