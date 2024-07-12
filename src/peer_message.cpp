#include "peer_message.hpp"

#include "utils.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <fstream>
#include <iostream>
#include <netinet/in.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace message
{

// Message

const std::vector<uint8_t> &Message::serialized() const
{
	return m_data;
}

Message::Message(std::vector<uint8_t> &&buffer)
	: m_data(std::move(buffer))
{
}

// KeepAlive

KeepAlive::KeepAlive()
	: Message{ { 0x00, 0x00, 0x00, 0x00 } }
{
}
KeepAlive::KeepAlive(std::vector<uint8_t> &&buffer)
	: Message{ std::move(buffer) }
{
}

// Choke

Choke::Choke(std::vector<uint8_t> &&buffer)
	: Message{ std::move(buffer) }
{
}
Choke::Choke()
	: Message{ { 0x00, 0x00, 0x00, 0x01, 0x00 } }
{
}

// Unchoke

Unchoke::Unchoke(std::vector<uint8_t> &&buffer)
	: Message{ std::move(buffer) }
{
}
Unchoke::Unchoke()
	: Message{ { 0x00, 0x00, 0x00, 0x01, 0x01 } }
{
}

// Interested

Interested::Interested(std::vector<uint8_t> &&buffer)
	: Message{ std::move(buffer) }
{
}
Interested::Interested()
	: Message{ { 0x00, 0x00, 0x00, 0x01, 0x02 } }
{
}

// NotInterested

NotInterested::NotInterested(std::vector<uint8_t> &&buffer)
	: Message{ std::move(buffer) }
{
}
NotInterested::NotInterested()
	: Message{ { 0x00, 0x00, 0x00, 0x01, 0x03 } }
{
}

// Have

Have::Have(std::vector<uint8_t> &&buffer)
	: Message{ std::move(buffer) }
{
}

Have::Have(uint32_t index)
	: Message{ [index]() mutable {
		std::vector<uint8_t> ret{ 0x00, 0x00, 0x00, 0x05, 0x04, 0x00, 0x00, 0x00, 0x00 };
		const uint32_t ind = htonl(index);
		memcpy(ret.data() + 4 + 1, &ind, sizeof ind);
		return ret;
	}() }
{
}

[[nodiscard]] uint32_t Have::get_index() const
{
	uint32_t index;
	memcpy(&index, m_data.data() + 4 + 1, sizeof index);
	return ntohl(index);
}
void Have::set_index(uint32_t index)
{
	index = htonl(index);
	memcpy(m_data.data() + 4 + 1, &index, sizeof index);
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

// Port

} // namespace message
