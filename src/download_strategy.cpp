#include "download_strategy.hpp"
#include "expected.hpp"
#include <algorithm>
#include <iostream>
#include <random>

DownloadStrategySequential::DownloadStrategySequential(size_t length)
	: m_bf(length)
	, m_endgame_pieces([length]() {
		std::set<size_t> ret;
		std::generate_n(std::inserter(ret, ret.end()), length,
				[i = 0]() mutable { return i++; });
		return ret;
	}())
	, m_gen([]() {
		std::random_device rd;
		return std::mt19937(rd());
	}())
{
}

bool DownloadStrategySequential::have_missing_pieces(const message::Bitfield &bitfield)
{
	if (!m_endgame)
	{
		bool found_spare_piece = false;
		for (size_t i = 0; i < m_bf.get_bf_size(); ++i)
		{
			if (!m_bf.get_index(i))
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
		return !m_bf.get_index(have.get_index());
	}
	return m_endgame_pieces.contains(have.get_index());
}

tl::expected<size_t, DownloadStrategy::ReturnStatus>
DownloadStrategySequential::next_piece_to_dl(const message::Bitfield &bitfield)
{
	if (!m_endgame)
	{
		bool found_spare_piece = false;
		size_t last_index = m_bf.get_bf_size() - 1;
		if (m_bf.get_index(last_index))
		{
			found_spare_piece = true;
			if (bitfield.get_index(last_index))
			{
				m_bf.set_index(last_index, true);
				std::clog << "Piece " << last_index << " will be downloaded"
					  << '\n';
				return last_index;
			}
		}
		for (size_t i = 0; i < m_bf.get_bf_size(); ++i)
		{
			if (!m_bf.get_index(i))
			{
				found_spare_piece = true;
				if (bitfield.get_index(i))
				{
					m_bf.set_index(i, true);
					std::clog << "Piece " << i << " will be downloaded" << '\n';
					return i;
				}
			}
		}
		if (found_spare_piece)
		{
			return tl::make_unexpected(ReturnStatus::NO_PIECE_FOUND);
		}
		m_endgame = true;
	}
	if (m_endgame_pieces.empty())
	{
		return tl::make_unexpected(ReturnStatus::DOWNLOAD_COMPLETED);
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
	m_bf.set_index(index, false);
}
