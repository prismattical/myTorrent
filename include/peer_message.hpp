#pragma once

#include "socket.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <netinet/in.h>
#include <string>
#include <tuple>
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
	Handshake(const Socket &socket, const std::string &info_hash,
		  const std::string &peer_id = "");
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
public:
	KeepAlive() = default;
	[[nodiscard]] std::vector<uint8_t> serialized() const;
};

struct Choke final {
private:
public:
	Choke() = default;
	[[nodiscard]] std::vector<uint8_t> serialized() const;
};

struct Unchoke final {
private:
public:
	Unchoke() = default;
	[[nodiscard]] std::vector<uint8_t> serialized() const;
};

struct Interested final {
private:
public:
	Interested() = default;
	[[nodiscard]] std::vector<uint8_t> serialized() const;
};

struct NotInterested final {
private:
public:
	NotInterested() = default;
	[[nodiscard]] std::vector<uint8_t> serialized() const;
};

struct Have final {
private:
	uint32_t m_index;

public:
	Have(uint32_t index);

	void index(uint32_t index);
	[[nodiscard]] uint32_t index() const;

	[[nodiscard]] std::vector<uint8_t> serialized() const;
};

struct Bitfield final {
private:
	std::vector<uint8_t> m_bitfield;

public:
	Bitfield() = default;
	/**
	 * @brief Create bitfield from received message
	 * 
	 * @param buffer received message
	 */
	Bitfield(std::vector<uint8_t> &&bitfield);

	/**
	* @brief Create empty bitfield ctor 
	* 
	* @param length an amount of pieces
	*/
	Bitfield(size_t length);
	/**
	 * @brief Check file and create bitfield ctor
	 */
	Bitfield(std::ifstream &file, long long piece_length, std::string_view hashes);

	void set_index(size_t index, bool value) noexcept;

	[[nodiscard]] std::tuple<std::vector<uint8_t>, const std::vector<uint8_t> &>
	serialized() const;
};

struct Request final {
private:
	uint32_t m_index;
	uint32_t m_begin;
	uint32_t m_length;

public:
	Request(uint32_t index, uint32_t begin, uint32_t length);

	[[nodiscard]] uint32_t get_index() const;
	void set_index(uint32_t index);
	[[nodiscard]] uint32_t get_begin() const;
	void set_begin(uint32_t begin);
	[[nodiscard]] uint32_t get_length() const;
	void set_length(uint32_t length);

	[[nodiscard]] std::vector<uint8_t> serialized() const;
};

struct Piece final {
private:
	uint32_t m_index;
	uint32_t m_begin;
	std::vector<uint8_t> m_block;

public:
	Piece(uint32_t index, uint32_t begin, std::vector<uint8_t> &&block);

	[[nodiscard]] uint32_t get_index() const;
	void set_index(uint32_t index);
	[[nodiscard]] uint32_t get_begin() const;
	void set_begin(uint32_t begin);

	[[nodiscard]] std::tuple<std::vector<uint8_t>, const std::vector<uint8_t> &>
	serialized() const;
};

struct Cancel final {
private:
	uint32_t m_index;
	uint32_t m_begin;
	uint32_t m_length;

public:
	Cancel(uint32_t index, uint32_t begin, uint32_t length);

	[[nodiscard]] uint32_t get_index() const;
	void set_index(uint32_t index);
	[[nodiscard]] uint32_t get_begin() const;
	void set_begin(uint32_t begin);
	[[nodiscard]] uint32_t get_length() const;
	void set_length(uint32_t length);

	[[nodiscard]] std::vector<uint8_t> serialized() const;
};

struct Port final {
private:
	uint16_t m_port;

public:
	Port(uint16_t port);

	[[nodiscard]] uint16_t get_port() const;
	void set_port(uint16_t port);

	[[nodiscard]] std::vector<uint8_t> serialized() const;
};

using Message = std::variant<KeepAlive, Choke, Unchoke, Interested, NotInterested, Have, Bitfield,
			     Request, Piece, Cancel, Port>;

Message read_message(const Socket &socket);

} // namespace message
