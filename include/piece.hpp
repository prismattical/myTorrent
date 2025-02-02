#pragma once

#include "peer_message.hpp"

#include <string>
#include <vector>

struct ReceivedPiece {
	friend class FileHandler;

public:
	std::vector<message::Piece> m_pieces;

	ReceivedPiece() = default;
	ReceivedPiece(ReceivedPiece &other) = delete;
	ReceivedPiece &operator=(ReceivedPiece &other) = delete;

	void add_block(message::Piece &&block);
	void clear();
	[[nodiscard]] size_t get_index() const;

	[[nodiscard]] std::string compute_sha1() const;
};
