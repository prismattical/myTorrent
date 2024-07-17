#include "peer_message.hpp"

#include "utils.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace message
{

// Handshake

Handshake::Handshake(const Socket &socket)
	: m_pstrlen([this, &socket]() {
		auto recv = socket.recv_some(0)[0];
		assert(recv == 19 && "invalid handshake received");
		return recv;
	}())
	, m_pstr([this, &socket]() {
		auto recv = socket.recv_some(m_pstrlen);
		std::string ret(recv.begin(), recv.end());
		assert(ret == "BitTorrent protocol" && "invalid handshake received");
		return ret;
	}())
	, m_reserved(socket.recv_some(8))
	, m_info_hash([&socket]() {
		auto recv = socket.recv_some(20);
		std::string ret(recv.begin(), recv.end());
		return ret;
	}())
	, m_peer_id([&socket]() {
		auto recv = socket.recv_some(20);
		std::string ret(recv.begin(), recv.end());
		return ret;
	}())
{
}
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

// Bitfield

Bitfield::Bitfield(std::vector<uint8_t> &&buffer)
	: Message{ std::move(buffer) }
{
}

Bitfield::Bitfield(const size_t length)
	: Message([length]() mutable {
		// (A - 1) / B + 1 - ceiling rounding for integer division
		const size_t bf_len = (length - 1) / 8 + 1;
		std::vector<uint8_t> ret(4 + 1 + bf_len, 0);
		const uint32_t len = htonl(bf_len + 1);

		memcpy(ret.data(), &len, sizeof len);
		ret[4] = 0x05;

		return ret;
	}())
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
		m_data[4 + 1 + index / 8] |= static_cast<uint8_t>(1) << (7 - index % 8);
	} else
	{
		m_data[4 + 1 + index / 8] &= ~(static_cast<uint8_t>(1) << (7 - index % 8));
	}
}

// Request

Request::Request(std::vector<uint8_t> &&buffer)
	: Message{ std::move(buffer) }
{
}

Request::Request(uint32_t index, uint32_t begin, uint32_t length)
	: Message{ [index, begin, length]() mutable {
		std::vector<uint8_t> ret{ 0x00, 0x00, 0x00, 0x0D, 0x06, 0x00, 0x00, 0x00, 0x00,
					  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

		index = htonl(index);
		begin = htonl(begin);
		length = htonl(length);

		memcpy(ret.data() + 4 + 1, &index, sizeof index);
		memcpy(ret.data() + 4 + 1 + 4, &begin, sizeof begin);
		memcpy(ret.data() + 4 + 1 + 4 + 4, &length, sizeof length);

		return ret;
	}() }
{
}

[[nodiscard]] uint32_t Request::get_index() const
{
	uint32_t index;
	memcpy(&index, m_data.data() + 4 + 1, sizeof index);
	return ntohl(index);
}
void Request::set_index(uint32_t index)
{
	index = htonl(index);
	memcpy(m_data.data() + 4 + 1, &index, sizeof index);
}

[[nodiscard]] uint32_t Request::get_begin() const
{
	uint32_t begin;
	memcpy(&begin, m_data.data() + 4 + 1 + 4, sizeof begin);
	return ntohl(begin);
}
void Request::set_begin(uint32_t begin)
{
	begin = htonl(begin);
	memcpy(m_data.data() + 4 + 1 + 4, &begin, sizeof begin);
}

[[nodiscard]] uint32_t Request::get_length() const
{
	uint32_t length;
	memcpy(&length, m_data.data() + 4 + 1 + 4 + 4, sizeof length);
	return ntohl(length);
}
void Request::set_length(uint32_t length)
{
	length = htonl(length);
	memcpy(m_data.data() + 4 + 1 + 4 + 4, &length, sizeof length);
}

// Piece

Piece::Piece(std::vector<uint8_t> &&buffer)
	: Message{ std::move(buffer) }
{
}

// Cancel

Cancel::Cancel(std::vector<uint8_t> &&buffer)
	: Message{ std::move(buffer) }
{
}

Cancel::Cancel(uint32_t index, uint32_t begin, uint32_t length)
	: Message{ [index, begin, length]() mutable {
		std::vector<uint8_t> ret{ 0x00, 0x00, 0x00, 0x0D, 0x08, 0x00, 0x00, 0x00, 0x00,
					  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

		index = htonl(index);
		begin = htonl(begin);
		length = htonl(length);

		memcpy(ret.data() + 4 + 1, &index, sizeof index);
		memcpy(ret.data() + 4 + 1 + 4, &begin, sizeof begin);
		memcpy(ret.data() + 4 + 1 + 4 + 4, &length, sizeof length);

		return ret;
	}() }
{
}

[[nodiscard]] uint32_t Cancel::get_index() const
{
	uint32_t index;
	memcpy(&index, m_data.data() + 4 + 1, sizeof index);
	return ntohl(index);
}
void Cancel::set_index(uint32_t index)
{
	index = htonl(index);
	memcpy(m_data.data() + 4 + 1, &index, sizeof index);
}

[[nodiscard]] uint32_t Cancel::get_begin() const
{
	uint32_t begin;
	memcpy(&begin, m_data.data() + 4 + 1 + 4, sizeof begin);
	return ntohl(begin);
}
void Cancel::set_begin(uint32_t begin)
{
	begin = htonl(begin);
	memcpy(m_data.data() + 4 + 1 + 4, &begin, sizeof begin);
}

[[nodiscard]] uint32_t Cancel::get_length() const
{
	uint32_t length;
	memcpy(&length, m_data.data() + 4 + 1 + 4 + 4, sizeof length);
	return ntohl(length);
}
void Cancel::set_length(uint32_t length)
{
	length = htonl(length);
	memcpy(m_data.data() + 4 + 1 + 4 + 4, &length, sizeof length);
}

// Port

Port::Port(std::vector<uint8_t> &&buffer)
	: Message(std::move(buffer))
{
}

Message read_message(const Socket &socket)
{
	uint32_t len = ntohl(socket.recv_length());
	if (len == 0)
	{
		return KeepAlive();
	}
	// TODO rest of the code
}

} // namespace message
