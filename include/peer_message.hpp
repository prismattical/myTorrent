#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <span>
#include <vector>

namespace message
{

class Message {
public:
	[[nodiscard]] virtual std::span<const uint8_t> serialized() const & = 0;
	[[nodiscard]] virtual std::span<const uint8_t> serialized() const && = delete;
	virtual ~Message() = default;
};

struct Handshake final : public Message {
private:
	std::array<uint8_t, 49 + 19> m_data{ "\x13"
					     "BitTorrent protocol" };

public:
	Handshake(std::span<const uint8_t> info_hash, std::span<const uint8_t> peer_id);
	Handshake(std::span<const uint8_t> handshake);

	[[nodiscard]] std::span<const uint8_t> serialized() const & override;

	[[nodiscard]] std::span<const uint8_t> get_pstrlen() const;

	[[nodiscard]] std::span<const uint8_t> get_pstr() const;

	[[nodiscard]] std::span<const uint8_t> get_reserved() const;

	void set_info_hash(std::span<const uint8_t> info_hash);
	[[nodiscard]] std::span<const uint8_t> get_info_hash() const;

	void set_peer_id(std::span<const uint8_t> peer_id);
	[[nodiscard]] std::span<const uint8_t> get_peer_id() const;
};

struct KeepAlive final : public Message {
private:
	std::array<uint8_t, 4> m_data{ 0, 0, 0, 0 };

public:
	KeepAlive() = default;
	[[nodiscard]] std::span<const uint8_t> serialized() const & override;
};

struct Choke final : public Message {
private:
	std::array<uint8_t, 5> m_data{ 0, 0, 0, 1, 0 };

public:
	Choke() = default;
	[[nodiscard]] std::span<const uint8_t> serialized() const & override;
};

struct Unchoke final : public Message {
private:
	std::array<uint8_t, 5> m_data{ 0, 0, 0, 1, 1 };

public:
	Unchoke() = default;
	[[nodiscard]] std::span<const uint8_t> serialized() const & override;
};

struct Interested final : public Message {
private:
	std::array<uint8_t, 5> m_data{ 0, 0, 0, 1, 2 };

public:
	Interested() = default;
	[[nodiscard]] std::span<const uint8_t> serialized() const & override;
};

struct NotInterested final : public Message {
private:
	std::array<uint8_t, 5> m_data{ 0, 0, 0, 1, 3 };

public:
	NotInterested() = default;
	[[nodiscard]] std::span<const uint8_t> serialized() const & override;
};

struct Have final : public Message {
private:
	std::array<uint8_t, 9> m_data{ 0, 0, 0, 5, 4 };

public:
	Have(uint32_t index);
	Have(std::span<const uint8_t> have);

	void set_index(uint32_t index);
	[[nodiscard]] uint32_t get_index() const;

	[[nodiscard]] std::span<const uint8_t> serialized() const & override;
};

struct Bitfield final : public Message {
private:
	std::vector<uint8_t> m_data;

	void set_message_length(uint32_t length);

public:
	Bitfield() = default;

	// creates bitfield from received message
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
	[[nodiscard]] bool get_index(size_t index) const noexcept;
	[[nodiscard]] size_t get_container_size() const;

	[[nodiscard]] std::span<const uint8_t> serialized() const & override;
};

struct Request final : public Message {
private:
	std::array<uint8_t, 17> m_data{ 0, 0, 0, 13, 6 };

public:
	Request(uint32_t index, uint32_t begin, uint32_t length);
	Request(std::span<const uint8_t> request);

	void set_index(uint32_t index);
	[[nodiscard]] uint32_t get_index() const;
	void set_begin(uint32_t begin);
	[[nodiscard]] uint32_t get_begin() const;
	void set_length(uint32_t length);
	[[nodiscard]] uint32_t get_length() const;

	[[nodiscard]] std::span<const uint8_t> serialized() const & override;
};

struct Piece final : public Message {
private:
	std::vector<uint8_t> m_data;

public:
	// creates piece from received message
	Piece(std::vector<uint8_t> &&piece);

	// creates piece from file
	Piece(std::ofstream &file, uint32_t index, uint32_t begin, uint32_t length);

	Piece(const Piece &) = delete; // make it non-copyable so any possible copy
	Piece &operator=(const Piece &) = delete; // will not go silent

	Piece(Piece &&other) noexcept = default;
	Piece &operator=(Piece &&) noexcept = default;

	void set_index(uint32_t index);
	[[nodiscard]] uint32_t get_index() const;
	void set_begin(uint32_t begin);
	[[nodiscard]] uint32_t get_begin() const;
	[[nodiscard]] uint32_t get_length() const;

	[[nodiscard]] std::span<const uint8_t> serialized() const & override;
};

struct Cancel final : public Message {
private:
	std::array<uint8_t, 17> m_data{ 0, 0, 0, 13, 8 };

public:
	Cancel(uint32_t index, uint32_t begin, uint32_t length);
	Cancel(std::span<const uint8_t> cancel);

	void set_index(uint32_t index);
	[[nodiscard]] uint32_t get_index() const;
	void set_begin(uint32_t begin);
	[[nodiscard]] uint32_t get_begin() const;
	void set_length(uint32_t length);
	[[nodiscard]] uint32_t get_length() const;

	[[nodiscard]] std::span<const uint8_t> serialized() const & override;
};

struct Port final : public Message {
private:
	std::array<uint8_t, 7> m_data{ 0, 0, 0, 3, 9 };

public:
	Port(uint16_t port);

	void set_port(uint16_t port);
	[[nodiscard]] uint16_t get_port() const;

	[[nodiscard]] std::span<const uint8_t> serialized() const & override;
};

} // namespace message
