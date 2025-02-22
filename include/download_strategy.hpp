#pragma once

#include "expected.hpp"
#include "peer_message.hpp"

#include <cstddef>
#include <cstdlib>
#include <random>
#include <set>

class DownloadStrategy {
public:
	enum class ReturnStatus {
		DOWNLOAD_COMPLETED,
		NO_PIECE_FOUND,
	};

	virtual bool have_missing_pieces(const message::Bitfield &bitfield) = 0;
	virtual bool is_piece_missing(const message::Have &have) = 0;

	virtual tl::expected<size_t, ReturnStatus>
	next_piece_to_dl(const message::Bitfield &bitfield) = 0;
	virtual void mark_as_downloaded(size_t index) = 0;
	virtual void mark_as_discarded(size_t index) = 0;
	virtual ~DownloadStrategy() = default;
};

class DownloadStrategySequential : public DownloadStrategy {
private:
	message::Bitfield m_bf;

	bool m_endgame = false;

	std::set<size_t> m_endgame_pieces;
	std::mt19937 m_gen;
	std::uniform_int_distribution<> m_distrib;

public:
	DownloadStrategySequential() = default;
	explicit DownloadStrategySequential(size_t length);

	bool have_missing_pieces(const message::Bitfield &bitfield) override;
	bool is_piece_missing(const message::Have &have) override;

	[[nodiscard]] tl::expected<size_t, ReturnStatus>
	next_piece_to_dl(const message::Bitfield &bitfield) override;
	void mark_as_downloaded(size_t index) override;
	void mark_as_discarded(size_t index) override;
};
