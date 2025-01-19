#pragma once

#include "metainfo_file.hpp"
#include "piece.hpp"

#include <set>

/**
 * @brief Class for doing file i/o
 * 
 * This is a handler to do any operations with files
 */
class FileHandler {
private:
	FileInfo m_fileinfo;
	std::set<size_t> m_pieces;
	long long m_left_offset;
	long long m_right_offset;

public:
	FileHandler(FileInfo fileinfo, std::set<size_t> pieces, long long left_offset,
		    long long right_offset);

	void mark_as_last_file();

	/**
	 * @return -1 if given piece is to the left to the file, 
	 * 0 if piece is part of the file, 
	 * 1 if piece is to the right to the file
	 */
	[[nodiscard]] int is_piece_part_of_file(size_t index) const;
	void preallocate_file(const std::filesystem::path &fdir_path) const;
	std::tuple<bool, size_t> read_piece(size_t index, std::vector<uint8_t> &piece,
					    const std::filesystem::path &fdir_path,
					    size_t piece_length) const;

	void write_piece(const ReceivedPiece &piece, const std::filesystem::path &fdir_path,
			 size_t piece_length) const;
};