#pragma once

#include "socket.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <netinet/in.h>
#include <string>
#include <variant>
#include <vector>

namespace message
{

struct Handshake final {
private:
	uint8_t m_pstrlen = 19;
	std::string m_pstr = "BitTorrent protocol";
	std::vector<uint8_t> m_reserved = std::vector<uint8_t>(8, 0);
	std::string m_info_hash;
	std::string m_peer_id;

public:
	Handshake() = default;
	Handshake(const Socket &socket);
	Handshake(const std::string &info_hash, const std::string &peer_id);

	[[nodiscard]] std::vector<uint8_t> serialized() const;

	[[nodiscard]] uint8_t pstrlen() const;

	void pstr(const std::string &pstr);
	[[nodiscard]] std::string pstr() const;

	void info_hash(const std::string &info_hash);
	[[nodiscard]] std::string info_hash() const;

	void peer_id(const std::string &peer_id);
	[[nodiscard]] std::string peer_id() const;
};

struct KeepAlive final {
private:
	uint32_t m_length = 0;

public:
	KeepAlive() = default;
	[[nodiscard]] std::vector<uint8_t> serialized() const;
};

struct Choke final {
private:
	uint32_t m_length = 1;
	uint8_t m_id = 0x00;

public:
	Choke() = default;
	[[nodiscard]] std::vector<uint8_t> serialized() const;
};

struct Unchoke final {
private:
	uint32_t m_length = 1;
	uint8_t m_id = 0x01;

public:
	Unchoke() = default;
	[[nodiscard]] std::vector<uint8_t> serialized() const;
};

struct Interested final {
private:
	uint32_t m_length = 1;
	uint8_t m_id = 0x02;

public:
	Interested() = default;
	[[nodiscard]] std::vector<uint8_t> serialized() const;
};

struct NotInterested final {
private:
	uint32_t m_length = 1;
	uint8_t m_id = 0x03;

public:
	NotInterested() = default;
	[[nodiscard]] std::vector<uint8_t> serialized() const;
};

struct Have final {
private:
	uint32_t m_length = 5;
	uint8_t m_id = 0x03;
	uint32_t m_index;

public:
	Have(uint32_t index);

	void index(uint32_t index);
	[[nodiscard]] uint32_t index() const;

	[[nodiscard]] std::vector<uint8_t> serialized() const;
};

struct Bitfield final : public Message {
	Bitfield() = default;
	Bitfield(std::vector<uint8_t> &&buffer);
	// length here is an amount of pieces, not the length of a file or any other length
	Bitfield(size_t length);
	Bitfield(std::ifstream &file, long long piece_length, std::string_view hashes);

	void set_index(size_t index, bool value) noexcept;
};

struct Request : public Message {
	Request(std::vector<uint8_t> &&buffer);
	Request(uint32_t index, uint32_t begin, uint32_t length);

	[[nodiscard]] uint32_t get_index() const;
	void set_index(uint32_t index);
	[[nodiscard]] uint32_t get_begin() const;
	void set_begin(uint32_t begin);
	[[nodiscard]] uint32_t get_length() const;
	void set_length(uint32_t length);
};

struct Piece final : public Message {
	Piece(std::vector<uint8_t> &&buffer);
};

// Cancel message is identical to Request with id changed from 6 to 8
// some sort of inheritance or typedef would look ugly
// so I chose code duplication instead
struct Cancel : public Message {
	Cancel(std::vector<uint8_t> &&buffer);
	Cancel(uint32_t index, uint32_t begin, uint32_t length);

	[[nodiscard]] uint32_t get_index() const;
	void set_index(uint32_t index);
	[[nodiscard]] uint32_t get_begin() const;
	void set_begin(uint32_t begin);
	[[nodiscard]] uint32_t get_length() const;
	void set_length(uint32_t length);
};

struct Port final : public Message {
	Port(std::vector<uint8_t> &&buffer);
};

using Message = std::variant<KeepAlive, Choke, Unchoke, Interested, NotInterested, Have, Bitfield,
			     Request, Piece, Cancel, Port>;

Message read_message(const Socket &socket);

} // namespace message
