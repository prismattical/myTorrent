#pragma once

#include "peer_message.hpp"

#include <cstddef>
#include <string>
#include <vector>

struct ReceivedPiece {
	friend class FileHandler;

private:
	std::vector<message::Piece> m_pieces;

public:
	ReceivedPiece() = default;
	~ReceivedPiece() = default;
	ReceivedPiece(ReceivedPiece &&) = default;
	ReceivedPiece &operator=(ReceivedPiece &&) = default;

	ReceivedPiece(const ReceivedPiece &) = delete;
	ReceivedPiece &operator=(const ReceivedPiece &) = delete;

	void add_block(message::Piece &&block);
	void clear();
	[[nodiscard]] size_t get_index() const;

	[[nodiscard]] std::string compute_sha1() const;
};
