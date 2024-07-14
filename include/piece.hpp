#pragma once

#include <cstdint>
#include <fstream>
#include <vector>

class Piece {
	std::vector<uint8_t> m_piece;

public:
	Piece() = default;
	Piece(long length);
	Piece(std::ifstream &file, long index, long length);
};
