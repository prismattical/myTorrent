#include "piece.hpp"

Piece::Piece(long length)
	: m_piece(length, 0)
{
}

Piece::Piece(std::ifstream &file, long index, long length)
	: m_piece(length, 0)
{
	file.seekg(index * length);
	file.read(reinterpret_cast<char *>(m_piece.data()), length);
}
