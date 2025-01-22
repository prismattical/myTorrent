#include "file_handler.hpp"

#include "config.hpp"

#include <fstream>

// File -------------------------------------------------------------------------------

FileHandler::FileHandler(FileInfo fileinfo, std::set<size_t> needed_pieces, long long left_offset,
			 long long right_offset)
	: m_fileinfo(std::move(fileinfo))
	, m_pieces(std::move(needed_pieces))
	, m_left_offset(left_offset)
	, m_right_offset(right_offset)
{
}

void FileHandler::mark_as_last_file()
{
	m_right_offset = 0;
}

int FileHandler::is_piece_part_of_file(size_t index) const
{
	if (index < *m_pieces.cbegin())
	{
		return -1;
	}
	if (index > *m_pieces.crbegin())
	{
		return 1;
	}
	return 0;
}

void FileHandler::preallocate_file(const std::filesystem::path &fdir_path) const
{
	namespace fs = std::filesystem;
	const fs::path full_path =
		config::get_path_to_downloads_dir() / fdir_path / m_fileinfo.path;
	if (!fs::exists(full_path))
	{
		fs::create_directories(full_path.parent_path());
		std::ofstream fout(full_path);
		fs::resize_file(full_path, m_fileinfo.length);
	}
}

std::tuple<bool, size_t> FileHandler::read_piece(size_t index, std::vector<uint8_t> &piece,
						 const std::filesystem::path &fdir_path,
						 const size_t piece_length) const
{
	namespace fs = std::filesystem;
	const fs::path full_path =
		config::get_path_to_downloads_dir() / fdir_path / m_fileinfo.path;

	std::ifstream fin(full_path);

	size_t bytes_to_read = piece_length;
	size_t offset_piece = 0;
	size_t offset_file = 0;
	bool ret = true;
	if (*m_pieces.cbegin() == index)
	{
		bytes_to_read -= m_left_offset;
		offset_piece = m_left_offset;
	}
	else
	{
		offset_file = piece_length - m_left_offset;
	}
	if (*m_pieces.crbegin() == index)
	{
		bytes_to_read -= m_right_offset;
		if (m_right_offset != 0)
		{
			ret = false;
		}
	}

	size_t diff = index - *m_pieces.cbegin();
	if (diff != 0)
	{
		offset_file += piece_length * (diff - 1);
	}

	fin.seekg(static_cast<std::char_traits<char>::off_type>(offset_file), std::ios::beg);

	fin.read(reinterpret_cast<char *>(piece.data() + offset_piece),
		 static_cast<std::streamsize>(bytes_to_read));

	return { ret, bytes_to_read };
}

void FileHandler::write_piece(const ReceivedPiece &piece, const std::filesystem::path &fdir_path,
			      size_t piece_length) const
{
	namespace fs = std::filesystem;
	const uint32_t index = piece.get_index();
	const fs::path full_path =
		config::get_path_to_downloads_dir() / fdir_path / m_fileinfo.path;

	std::ofstream fout(full_path, std::ios::in | std::ios::out | std::ios::binary);

	size_t offset_piece = 0;
	size_t offset_file = 0;

	if (index == *m_pieces.cbegin())
	{
		offset_piece = m_left_offset;
	}
	else
	{
		offset_file = piece_length - m_left_offset;
	}

	size_t diff = index - *m_pieces.cbegin();
	if (diff != 0)
	{
		offset_file += piece_length * (diff - 1);
	}

	fout.seekp(static_cast<std::char_traits<char>::off_type>(offset_file), std::ios::beg);

	const size_t fb_len = piece.m_pieces[0].get_length();

	const size_t start = offset_piece;
	const size_t end = offset_piece + m_fileinfo.length;
	size_t current = 0;
	for (const auto &block : piece.m_pieces)
	{
		if (current + block.get_length() <= start)
		{
			current += block.get_length();
			continue;
		}
		if (current >= end)
		{
			break;
		}

		size_t possible_offset_left = current < start ? start % fb_len : 0;
		size_t possible_offset_right =
			current + block.get_length() >= end ? block.get_length() - end % fb_len : 0;
		size_t length = block.get_length() - possible_offset_left - possible_offset_right;

		fout.write(reinterpret_cast<const char *>(block.get_data().data() +
							  possible_offset_left),
			   static_cast<std::streamsize>(length));
		fout.flush();

		current += block.get_length();
	}
	fout.close();
}
