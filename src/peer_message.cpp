#include "peer_message.hpp"

#include "utils.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
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
	uint32_t index;
	memcpy(&index, m_data.data() + 4 + 1, sizeof index);
	return ntohl(index);
}

std::span<const uint8_t> Have::serialized() const &
{
	return m_data;
}

// Bitfield

void Bitfield::set_message_length(uint32_t length)
{
	length = htonl(length);
	memcpy(m_data.data(), &length, sizeof length);
}

Bitfield::Bitfield(std::vector<uint8_t> &&bitfield)
	: m_data{ std::move(bitfield) }
{
}

Bitfield::Bitfield(const size_t length)
	: m_data(5 + ((length - 1) / 8 + 1), 0)
{
	set_message_length(m_data.size() - 4);
	m_data[4] = 5;
}

Bitfield::Bitfield(std::ifstream &file, const long long piece_length, const std::string_view hashes)
	: Bitfield(5 + hashes.length() / 20)
{
	set_message_length(m_data.size() - 4);
	m_data[4] = 5;

	std::vector<unsigned char> buffer(piece_length);
	for (size_t i = 0; i < hashes.size() / 20; i += 20)
	{
		// reinterpret_cast should be safe in this exact situation
		file.read(reinterpret_cast<char *>(buffer.data()), piece_length);
		const auto sha1 = utils::calculate_sha1(buffer);
		const std::string_view hash = hashes.substr(i, 20);
		bool is_equal = std::equal(sha1.begin(), sha1.end(), hash.begin(), hash.end());

		set_index(i / 20, is_equal);
	}
}

void Bitfield::set_index(const size_t index, const bool value) noexcept
{
	if (value)
	{
		m_data[5 + index / 8] |= static_cast<uint8_t>(1) << (7 - index % 8);
	} else
	{
		m_data[5 + index / 8] &= ~(static_cast<uint8_t>(1) << (7 - index % 8));
	}
}

bool Bitfield::get_index(const size_t index) const noexcept
{
	return (m_data[5 + index / 8] & static_cast<uint8_t>(1) << (7 - index % 8)) != 0;
}

size_t Bitfield::get_container_size() const
{
	return m_data.size();
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
	uint32_t index;
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
	uint32_t begin;
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
	uint32_t length;
	memcpy(&length, m_data.data() + 4 + 1 + 4 + 4, sizeof length);
	return ntohl(length);
}

std::span<const uint8_t> Request::serialized() const &
{
	return m_data;
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
	uint32_t index;
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
	uint32_t begin;
	memcpy(&begin, m_data.data() + 4 + 1 + 4, sizeof begin);
	return ntohl(begin);
}

uint32_t Piece::get_length() const
{
	return m_data.size() - 13;
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
	uint32_t index;
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
	uint32_t begin;
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
	uint32_t length;
	memcpy(&length, m_data.data() + 4 + 1 + 4 + 4, sizeof length);
	return ntohl(length);
}

std::span<const uint8_t> Cancel::serialized() const &
{
	return m_data;
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
	uint16_t port;
	memcpy(&port, m_data.data() + 4 + 1, sizeof port);
	return ntohs(port);
}

std::span<const uint8_t> Port::serialized() const &
{
	return m_data;
}

} // namespace message
