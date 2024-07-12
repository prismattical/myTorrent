#pragma once

#include <arpa/inet.h>
#include <fstream>
#include <netinet/in.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace message
{

// Inheritance just for the sake of avoiding code duplication and boilerplate

struct Message {
protected:
	std::vector<uint8_t> m_data;

	Message(std::vector<uint8_t> &&buffer);
	Message() = default;

public:
	[[nodiscard]] const std::vector<uint8_t> &serialized() const;
};

struct KeepAlive final : public Message {
	KeepAlive(std::vector<uint8_t> &&buffer);
	KeepAlive();
};

struct Choke final : public Message {
	Choke(std::vector<uint8_t> &&buffer);
	Choke();
};

struct Unchoke final : public Message {
	Unchoke(std::vector<uint8_t> &&buffer);
	Unchoke();
};

struct Interested final : public Message {
	Interested(std::vector<uint8_t> &&buffer);
	Interested();
};

struct NotInterested final : public Message {
	NotInterested(std::vector<uint8_t> &&buffer);
	NotInterested();
};

struct Have final : public Message {
	Have(std::vector<uint8_t> &&buffer);
	Have(uint32_t index);

	[[nodiscard]] uint32_t get_index() const;
	void set_index(uint32_t index);
};

struct Bitfield final : public Message {
private:
	void set_index(size_t index, bool value) noexcept;

public:
	Bitfield() = default;
	Bitfield(std::vector<uint8_t> &&buffer);
	// length here is an amount of pieces, not the length of a file or any other length
	Bitfield(size_t length);
	Bitfield(std::ifstream &file, long long piece_length, std::string_view hashes);

	// Bitfield logic
};

struct Request final : public Message {
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
	// TODO read from file ctor
};

struct Cancel final : public Message {};

struct Port final : public Message {};

} // namespace message
