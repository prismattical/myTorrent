#include "peer_message.hpp"

#include "utils.hpp"

#include <arpa/inet.h>
#include <cstddef>
#include <netinet/in.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace message
{

// Handshake

Handshake::Handshake(const std::string &info_hash, const std::string &peer_id)
	: m_info_hash{ info_hash }
	, m_peer_id{ peer_id }
{
}

std::vector<uint8_t> Handshake::serialized() const
{
	std::vector<uint8_t> ret(49 + m_pstrlen, 0);
	ret[0] = m_pstrlen;
	std::copy(m_pstr.begin(), m_pstr.end(), ret.begin() + 1);
	std::copy(m_reserved.begin(), m_reserved.end(), ret.begin() + 1 + m_pstrlen);
	std::copy(m_info_hash.begin(), m_info_hash.end(), ret.begin() + 1 + m_pstrlen + 8);
	std::copy(m_peer_id.begin(), m_peer_id.end(), ret.begin() + 1 + m_pstrlen + 8 + 20);
	return ret;
}

uint8_t Handshake::pstrlen() const
{
	return m_pstrlen;
}

void Handshake::pstr(const std::string &pstr)
{
	m_pstrlen = pstr.size();
	m_pstr = pstr;
}
std::string Handshake::pstr() const
{
	return m_pstr;
}

void Handshake::info_hash(const std::string &info_hash)
{
	m_info_hash = info_hash;
}
std::string Handshake::info_hash() const
{
	return m_info_hash;
}

void Handshake::peer_id(const std::string &peer_id)
{
	m_peer_id = peer_id;
}
std::string Handshake::peer_id() const
{
	return m_peer_id;
}

// KeepAlive

std::vector<uint8_t> KeepAlive::serialized() const
{
	return { 0x00, 0x00, 0x00, 0x00 };
}

// Choke

std::vector<uint8_t> Choke::serialized() const
{
	return { 0x00, 0x00, 0x00, 0x01, 0x00 };
}

// Unchoke

std::vector<uint8_t> Unchoke::serialized() const
{
	return { 0x00, 0x00, 0x00, 0x01, 0x01 };
}

// Interested

std::vector<uint8_t> Interested::serialized() const
{
	return { 0x00, 0x00, 0x00, 0x01, 0x02 };
}

// NotInterested

std::vector<uint8_t> NotInterested::serialized() const
{
	return { 0x00, 0x00, 0x00, 0x01, 0x03 };
}

// Have

Have::Have(uint32_t index)
	: m_index{ index }
{
}

void Have::index(uint32_t index)
{
	m_index = index;
}
uint32_t Have::index() const
{
	return m_index;
}

std::vector<uint8_t> Have::serialized() const
{
	std::vector<uint8_t> ret{ 0x00, 0x00, 0x00, 0x05, 0x04, 0x00, 0x00, 0x00, 0x00 };
	const uint32_t index = htonl(m_index);
	memcpy(ret.data() + 4 + 1, &index, sizeof index);
	return ret;
}

// Bitfield

Bitfield::Bitfield(std::vector<uint8_t> &&buffer)
	: m_bitfield{ std::move(buffer) }
{
}

Bitfield::Bitfield(const size_t length)
	: m_bitfield((length - 1) / 8 + 1, 0)
{
}

Bitfield::Bitfield(std::ifstream &file, const long long piece_length, const std::string_view hashes)
	: Bitfield(hashes.length() / 20)
{
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

void Bitfield::set_index(size_t index, bool value) noexcept
{
	if (value)
	{
		m_bitfield[index / 8] |= static_cast<uint8_t>(1) << (7 - index % 8);
	} else
	{
		m_bitfield[index / 8] &= ~(static_cast<uint8_t>(1) << (7 - index % 8));
	}
}

std::tuple<std::vector<uint8_t>, const std::vector<uint8_t> &> Bitfield::serialized() const
{
	std::vector<uint8_t> prefix(5, 0);
	prefix[4] = 0x05;
	const uint32_t len = htonl(1 + m_bitfield.size());
	memcpy(prefix.data(), &len, sizeof len);
	return { prefix, m_bitfield };
}

// Request

Request::Request(uint32_t index, uint32_t begin, uint32_t length)
	: m_index{ index }
	, m_begin{ begin }
	, m_length{ length }
{
}
uint32_t Request::get_index() const
{
	return m_index;
}
void Request::set_index(uint32_t index)
{
	m_index = index;
}
uint32_t Request::get_begin() const
{
	return m_begin;
}
void Request::set_begin(uint32_t begin)
{
	m_begin = begin;
}
uint32_t Request::get_length() const
{
	return m_length;
}
void Request::set_length(uint32_t length)
{
	m_length = length;
}

std::vector<uint8_t> Request::serialized() const
{
	std::vector<uint8_t> ret{ 0x00, 0x00, 0x00, 0x0D, 0x06, 0x00, 0x00, 0x00, 0x00,
				  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

	const uint32_t index = htonl(m_index);
	const uint32_t begin = htonl(m_begin);
	const uint32_t length = htonl(m_length);

	memcpy(ret.data() + 4 + 1, &index, sizeof index);
	memcpy(ret.data() + 4 + 1 + 4, &begin, sizeof begin);
	memcpy(ret.data() + 4 + 1 + 4 + 4, &length, sizeof length);

	return ret;
}

// Piece

Piece::Piece(uint32_t index, uint32_t begin, std::vector<uint8_t> &&block)
	: m_index(index)
	, m_begin(begin)
	, m_block(std::move(block))
{
}

uint32_t Piece::get_index() const
{
	return m_index;
}
void Piece::set_index(uint32_t index)
{
	m_index = index;
}
uint32_t Piece::get_begin() const
{
	return m_begin;
}
void Piece::set_begin(uint32_t begin)
{
	m_begin = begin;
}

std::tuple<std::vector<uint8_t>, const std::vector<uint8_t> &> Piece::serialized() const
{
	std::vector<uint8_t> prefix(13, 0);
	prefix[4] = 0x07;
	const uint32_t length = htonl(9 + m_block.size());
	const uint32_t index = htonl(m_index);
	const uint32_t begin = htonl(m_begin);
	memcpy(prefix.data(), &length, sizeof length);
	memcpy(prefix.data(), &index, sizeof index);
	memcpy(prefix.data(), &begin, sizeof begin);

	return { prefix, m_block };
}

// Cancel

Cancel::Cancel(uint32_t index, uint32_t begin, uint32_t length)
	: m_index{ index }
	, m_begin{ begin }
	, m_length{ length }
{
}
uint32_t Cancel::get_index() const
{
	return m_index;
}
void Cancel::set_index(uint32_t index)
{
	m_index = index;
}
uint32_t Cancel::get_begin() const
{
	return m_begin;
}
void Cancel::set_begin(uint32_t begin)
{
	m_begin = begin;
}
uint32_t Cancel::get_length() const
{
	return m_length;
}
void Cancel::set_length(uint32_t length)
{
	m_length = length;
}

std::vector<uint8_t> Cancel::serialized() const
{
	std::vector<uint8_t> ret{ 0x00, 0x00, 0x00, 0x0D, 0x08, 0x00, 0x00, 0x00, 0x00,
				  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

	const uint32_t index = htonl(m_index);
	const uint32_t begin = htonl(m_begin);
	const uint32_t length = htonl(m_length);

	memcpy(ret.data() + 4 + 1, &index, sizeof index);
	memcpy(ret.data() + 4 + 1 + 4, &begin, sizeof begin);
	memcpy(ret.data() + 4 + 1 + 4 + 4, &length, sizeof length);

	return ret;
}

// Port

Port::Port(uint16_t port)
	: m_port(port)
{
}

uint16_t Port::get_port() const
{
	return m_port;
}
void Port::set_port(uint16_t port)
{
	m_port = port;
}

std::vector<uint8_t> Port::serialized() const
{
	std::vector<uint8_t> ret{ 0x00, 0x00, 0x00, 0x03, 0x09, 0x00, 0x00 };
	const uint16_t port = htons(m_port);
	memcpy(ret.data() + 4 + 1, &port, sizeof port);
	return ret;
}

Message read_message(const Socket &socket)
{
	const uint32_t len = ntohl(socket.recv_length());
	if (len == 0)
	{
		return KeepAlive();
	} else
	{
		const uint8_t id = socket.recv_some(len)[0];
		switch (id)
		{
		case 0x00:
			return Choke();
		case 0x01:
			return Unchoke();
		case 0x02:
			return Interested();
		case 0x03:
			return NotInterested();
		case 0x04: {
			std::vector<uint8_t> buffer = socket.recv_some(len - 1);
			uint32_t index;
			memcpy(&index, buffer.data(), sizeof index);
			index = ntohl(index);
			return Have(index);
		}
		case 0x05: {
			std::vector<uint8_t> buffer = socket.recv_some(len - 1);
			return Bitfield(std::move(buffer));
		}
		case 0x06: {
			std::vector<uint8_t> buffer = socket.recv_some(len - 1);
			uint32_t index;
			uint32_t begin;
			uint32_t length;
			memcpy(&index, buffer.data(), sizeof index);
			memcpy(&begin, buffer.data() + 4, sizeof begin);
			memcpy(&length, buffer.data() + 4 + 4, sizeof length);
			index = ntohl(index);
			begin = ntohl(begin);
			length = ntohl(length);

			return Request(index, begin, length);
		}
		case 0x07: {
			std::vector<uint8_t> prefix = socket.recv_some(4 + 4);
			uint32_t index;
			uint32_t begin;
			memcpy(&index, prefix.data(), sizeof index);
			memcpy(&begin, prefix.data() + 4, sizeof begin);
			index = ntohl(index);
			begin = ntohl(begin);
			std::vector<uint8_t> buffer = socket.recv_some(len - 1 - 4 - 4);

			return Piece(index, begin, std::move(buffer));
		}
		case 0x08: {
			std::vector<uint8_t> buffer = socket.recv_some(len - 1);
			uint32_t index;
			uint32_t begin;
			uint32_t length;
			memcpy(&index, buffer.data(), sizeof index);
			memcpy(&begin, buffer.data() + 4, sizeof begin);
			memcpy(&length, buffer.data() + 4 + 4, sizeof length);
			index = ntohl(index);
			begin = ntohl(begin);
			length = ntohl(length);

			return Cancel(index, begin, length);
		}
		case 0x09: {
			std::vector<uint8_t> buffer = socket.recv_some(len - 1);
			uint16_t port;
			memcpy(&port, buffer.data(), sizeof port);
			return Port(port);
		}
		default:
			throw std::runtime_error("Unexpected id value");
		}
	}
}

} // namespace message
