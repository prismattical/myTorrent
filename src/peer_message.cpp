#include "peer_message.hpp"

#include <arpa/inet.h>
#include <array>
#include <netinet/in.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>

#include <utility>
#include <vector>

namespace message
{

// Handshake

Handshake::Handshake(const std::span<const uint8_t> info_hash,
		     const std::span<const uint8_t> peer_id)
{
	std::copy(info_hash.begin(), info_hash.end(), m_data.begin() + 1 + 19 + 8);
	std::copy(peer_id.begin(), peer_id.end(), m_data.begin() + 1 + 19 + 8 + 20);
}

Handshake::Handshake(std::span<const uint8_t> handshake)
{
	std::copy(handshake.begin(), handshake.end(), m_data.begin());
}

std::span<const uint8_t> Handshake::serialized() const &
{
	return m_data;
}

std::span<const uint8_t> Handshake::get_pstrlen() const
{
	return { m_data.begin(), 1 };
}

std::span<const uint8_t> Handshake::get_pstr() const
{
	return { m_data.begin() + 1, 19 };
}

std::span<const uint8_t> Handshake::get_reserved() const
{
	return { m_data.begin() + 1 + 19, 8 };
}

void Handshake::set_info_hash(std::span<const uint8_t> info_hash)
{
	std::copy(info_hash.begin(), info_hash.end(), m_data.begin() + 1 + 19 + 8);
}

std::span<const uint8_t> Handshake::get_info_hash() const
{
	return { m_data.begin() + 1 + 19 + 8, 20 };
}

void Handshake::set_peer_id(std::span<const uint8_t> peer_id)
{
	std::copy(peer_id.begin(), peer_id.end(), m_data.begin() + 1 + 19 + 8 + 20);
}
std::span<const uint8_t> Handshake::get_peer_id() const
{
	return { m_data.begin() + 1 + 19 + 8 + 20, 20 };
}

bool Handshake::is_valid(std::span<const uint8_t> info_hash)
{
	const char *boilerplate = "\x13"
				  "BitTorrent protocol";
	// part 1
	const bool p1 = std::equal(m_data.begin(), m_data.begin() + 1 + 19, boilerplate);
	// part 2
	const bool p2 = std::equal(m_data.begin() + 1 + 19 + 8, m_data.begin() + 1 + 19 + 8 + 20,
				   info_hash.begin());
	return p1 && p2;
}

// KeepAlive

std::span<const uint8_t> KeepAlive::serialized() const &
{
	return m_data;
}

// Choke

std::span<const uint8_t> Choke::serialized() const &
{
	return m_data;
}

// Unchoke

std::span<const uint8_t> Unchoke::serialized() const &
{
	return m_data;
}

// Interested

std::span<const uint8_t> Interested::serialized() const &
{
	return m_data;
}

// NotInterested

std::span<const uint8_t> NotInterested::serialized() const &
{
	return m_data;
}

// Have

Have::Have(std::span<const uint8_t> have)
{
	std::copy(have.begin(), have.end(), m_data.begin());
}

Have::Have(uint32_t index)
{
	set_index(index);
}

void Have::set_index(uint32_t index)
{
	index = htonl(index);
	memcpy(m_data.data() + 4 + 1, &index, sizeof index);
}
uint32_t Have::get_index() const
{
	uint32_t index = 0;
	memcpy(&index, m_data.data() + 4 + 1, sizeof index);
	return ntohl(index);
}

std::span<const uint8_t> Have::serialized() const &
{
	return m_data;
}

// Bitfield

Bitfield::Bitfield(std::span<const uint8_t> bitfield, const size_t supposed_length)
	: m_bitfield_length(supposed_length)
	, m_data{ bitfield.begin(), bitfield.end() }
{
	// todo: validate bitfield
	std::span<const uint8_t> bf = get_bf();
	size_t expected_size = (supposed_length + 8 - 1) / 8;
	if (expected_size != bf.size())
	{
		throw std::runtime_error("Invalid bf size");
	}

	for (size_t i = supposed_length; i < bf.size() * 8; ++i)
	{
		if (get_index(i))
		{
			throw std::runtime_error("Invalid bf trailing bits");
		}
	}
}

Bitfield::Bitfield(const size_t length)
	: m_bitfield_length(length)
	, m_data(5 + ((length - 1) / 8 + 1), 0)
{
	set_message_length(m_data.size() - 4);
	m_data[4] = 5;
}

void Bitfield::set_message_length(uint32_t length)
{
	length = htonl(length);
	memcpy(m_data.data(), &length, sizeof length);
}

uint32_t Bitfield::get_message_length() const
{
	uint32_t ret = 0;
	memcpy(&ret, m_data.data(), sizeof ret);
	return ntohl(ret);
}

std::span<const uint8_t> Bitfield::get_bf() const
{
	return { m_data.data() + 5, m_data.size() - 5 };
}
std::span<uint8_t> Bitfield::get_bf()
{
	return { m_data.data() + 5, m_data.size() - 5 };
}

void Bitfield::set_index(const size_t index, const bool value)
{
	if (value)
	{
		m_data[5 + index / 8] |= static_cast<uint8_t>(1) << (7 - index % 8);
	}
	else
	{
		m_data[5 + index / 8] &= ~(static_cast<uint8_t>(1) << (7 - index % 8));
	}
}

bool Bitfield::get_index(const size_t index) const
{
	return (m_data[5 + index / 8] & static_cast<uint8_t>(1) << (7 - index % 8)) != 0;
}

size_t Bitfield::get_msg_size() const
{
	return m_data.size();
}

size_t Bitfield::get_bf_size() const
{
	return m_bitfield_length;
}

std::span<const uint8_t> Bitfield::serialized() const &
{
	return m_data;
}

// Request

Request::Request(std::span<const uint8_t> request)
{
	std::copy(request.begin(), request.end(), m_data.begin());
}

Request::Request(uint32_t index, uint32_t begin, uint32_t length)
{
	set_index(index);
	set_begin(begin);
	set_length(length);
}
void Request::set_index(uint32_t index)
{
	index = htonl(index);
	memcpy(m_data.data() + 4 + 1, &index, sizeof index);
}
uint32_t Request::get_index() const
{
	uint32_t index = 0;
	memcpy(&index, m_data.data() + 4 + 1, sizeof index);
	return ntohl(index);
}
void Request::set_begin(uint32_t begin)
{
	begin = htonl(begin);
	memcpy(m_data.data() + 4 + 1 + 4, &begin, sizeof begin);
}
uint32_t Request::get_begin() const
{
	uint32_t begin = 0;
	memcpy(&begin, m_data.data() + 4 + 1 + 4, sizeof begin);
	return ntohl(begin);
}
void Request::set_length(uint32_t length)
{
	length = htonl(length);
	memcpy(m_data.data() + 4 + 1 + 4 + 4, &length, sizeof length);
}
uint32_t Request::get_length() const
{
	uint32_t length = 0;
	memcpy(&length, m_data.data() + 4 + 1 + 4 + 4, sizeof length);
	return ntohl(length);
}

std::span<const uint8_t> Request::serialized() const &
{
	return m_data;
}

message::Cancel Request::create_cancel() const
{
	std::array<uint8_t, 17> ret = m_data;
	ret[4] = 8;
	return message::Cancel(std::span<const uint8_t>(ret.data(), ret.size()));
}

// Piece

Piece::Piece(std::vector<uint8_t> &&piece)
	: m_data(std::move(piece))
{
}

void Piece::set_index(uint32_t index)
{
	index = htonl(index);
	memcpy(m_data.data() + 4 + 1, &index, sizeof index);
}
uint32_t Piece::get_index() const
{
	uint32_t index = 0;
	memcpy(&index, m_data.data() + 4 + 1, sizeof index);
	return ntohl(index);
}
void Piece::set_begin(uint32_t begin)
{
	begin = htonl(begin);
	memcpy(m_data.data() + 4 + 1 + 4, &begin, sizeof begin);
}
uint32_t Piece::get_begin() const
{
	uint32_t begin = 0;
	memcpy(&begin, m_data.data() + 4 + 1 + 4, sizeof begin);
	return ntohl(begin);
}

uint32_t Piece::get_length() const
{
	return m_data.size() - 13;
}

[[nodiscard]] std::span<const uint8_t> Piece::get_data() const
{
	return { m_data.data() + 13, m_data.size() - 13 };
}

std::span<const uint8_t> Piece::serialized() const &
{
	return m_data;
}

// Cancel
Cancel::Cancel(std::span<const uint8_t> cancel)
{
	std::copy(cancel.begin(), cancel.end(), m_data.begin());
}

Cancel::Cancel(uint32_t index, uint32_t begin, uint32_t length)
{
	set_index(index);
	set_begin(begin);
	set_length(length);
}
void Cancel::set_index(uint32_t index)
{
	index = htonl(index);
	memcpy(m_data.data() + 4 + 1, &index, sizeof index);
}
uint32_t Cancel::get_index() const
{
	uint32_t index = 0;
	memcpy(&index, m_data.data() + 4 + 1, sizeof index);
	return ntohl(index);
}
void Cancel::set_begin(uint32_t begin)
{
	begin = htonl(begin);
	memcpy(m_data.data() + 4 + 1 + 4, &begin, sizeof begin);
}
uint32_t Cancel::get_begin() const
{
	uint32_t begin = 0;
	memcpy(&begin, m_data.data() + 4 + 1 + 4, sizeof begin);
	return ntohl(begin);
}
void Cancel::set_length(uint32_t length)
{
	length = htonl(length);
	memcpy(m_data.data() + 4 + 1 + 4 + 4, &length, sizeof length);
}
uint32_t Cancel::get_length() const
{
	uint32_t length = 0;
	memcpy(&length, m_data.data() + 4 + 1 + 4 + 4, sizeof length);
	return ntohl(length);
}

std::span<const uint8_t> Cancel::serialized() const &
{
	return m_data;
}

message::Request Cancel::create_request() const
{
	std::array<uint8_t, 17> ret = m_data;
	ret[4] = 6;
	return message::Request(std::span<uint8_t>(ret));
}

// Port

Port::Port(uint16_t port)
{
	set_port(port);
}

void Port::set_port(uint16_t port)
{
	port = htons(port);
	memcpy(m_data.data() + 4 + 1, &port, sizeof port);
}
uint16_t Port::get_port() const
{
	uint16_t port = 0;
	memcpy(&port, m_data.data() + 4 + 1, sizeof port);
	return ntohs(port);
}

std::span<const uint8_t> Port::serialized() const &
{
	return m_data;
}

} // namespace message
