#include "download_strategy.hpp"
#include <algorithm>
#include <random>

DownloadStrategySequential::DownloadStrategySequential(size_t length)
	: m_pieces_downloading(length)
	, m_endgame_pieces([length]() {
		std::set<size_t> ret;
		std::generate_n(std::inserter(ret, ret.end()), length,
				[i = 0]() mutable { return i++; });
		return ret;
	}())
{
}

bool DownloadStrategySequential::have_missing_pieces(const message::Bitfield &bitfield)
{
	if (!m_endgame)
	{
		bool found_spare_piece = false;
		for (size_t i = 0; i < m_pieces_downloading.size(); ++i)
		{
			if (!m_pieces_downloading[i])
			{
				found_spare_piece = true;
				if (bitfield.get_index(i))
				{
					return true;
				}
			}
		}
		if (found_spare_piece)
		{
			return false;
		}
		m_endgame = true;
	}
	return std::any_of(m_endgame_pieces.begin(), m_endgame_pieces.end(),
			   [&bitfield](size_t piece) { return bitfield.get_index(piece); });
}

bool DownloadStrategySequential::is_piece_missing(const message::Have &have)
{
	if (!m_endgame)
	{
		if (std::find(m_pieces_downloading.begin(), m_pieces_downloading.end(), false) !=
		    m_pieces_downloading.end())
		{
			return !m_pieces_downloading[have.get_index()];
		}
		m_endgame = true;
	}
	return m_endgame_pieces.contains(have.get_index());
}

size_t DownloadStrategySequential::next_piece_to_dl(const message::Bitfield &bitfield)
{
	if (!m_endgame)
	{
		bool found_spare_piece = false;
		for (size_t i = 0; i < m_pieces_downloading.size(); ++i)
		{
			if (!m_pieces_downloading[i])
			{
				found_spare_piece = true;
				if (bitfield.get_index(i))
				{
					m_pieces_downloading[i] = true;
					return i;
				}
			}
		}
		if (found_spare_piece)
		{
			return m_pieces_downloading.size();
		}
		m_endgame = true;
		std::random_device rd;
		m_gen = std::mt19937(rd());
	}
	if (m_endgame_pieces.empty())
	{
		return m_pieces_downloading.size();
	}
	m_distrib =
		std::uniform_int_distribution<>(0, static_cast<int>(m_endgame_pieces.size()) - 1);
	int rand_index = m_distrib(m_gen);
	auto iter = std::next(m_endgame_pieces.begin(), rand_index);
	return *iter;
}

void DownloadStrategySequential::mark_as_downloaded(const size_t index)
{
	m_endgame_pieces.erase(index);
}

void DownloadStrategySequential::mark_as_discarded(const size_t index)
{
	m_pieces_downloading[index] = false;
}
