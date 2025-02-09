#include "config.hpp"
#include "download_strategy.hpp"
#include "expected.hpp"

#include "file_handler.hpp"
#include "peer_message.hpp"
#include "piece.hpp"
#include <gtest/gtest.h>
#include <utility>

class StrategyTest : public ::testing::Test {
protected:
	static constexpr size_t len = 100;
	message::Bitfield full_bf;
	message::Bitfield partial_bf;

public:
	void SetUp() override
	{
		full_bf = message::Bitfield(len);
		partial_bf = message::Bitfield(len);
		for (size_t i = 0; i < len; ++i)
		{
			full_bf.set_index(i, true);
		}
		for (size_t i = 0; i < len / 2; ++i)
		{
			partial_bf.set_index(i, true);
		}
	}
};

TEST_F(StrategyTest, PeekingTest)
{
	auto dl_strt = DownloadStrategySequential(len);
	const message::Have have_false(0);
	const message::Have have_true(1);

	auto ind = dl_strt.next_piece_to_dl(full_bf);

	dl_strt.mark_as_discarded(ind.value());

	// * This test is meant to be viewed in debugger with step-by-step execution
	// * of each line of code. It's not about fully validating the algorithm but rather
	// * clarifying that no unexpected things happen

	for (size_t i = 0; i < len - 10; ++i)
	{
		ind = dl_strt.next_piece_to_dl(full_bf);
		dl_strt.mark_as_downloaded(ind.value());
		EXPECT_EQ(ind, i);
	}

	for (size_t i = 0; i < 20; ++i)
	{
		ind = dl_strt.next_piece_to_dl(full_bf);
	}
	for (size_t i = len - 1; i >= len - 1 - 10; --i)
	{
		dl_strt.mark_as_downloaded(i);
	}
}

TEST_F(StrategyTest, OtherTest)
{
	config::load_configs();

	message::Piece piece1{ { 0, 0, 0, 19, 7, 0, 0, 0, 0, 0, 0, 0,
				 0, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1 } };
	message::Piece piece2{ { 0,  0, 0, 19, 7, 0, 0, 0, 0, 0, 0, 0,
				 10, 2, 2, 2,  2, 2, 2, 2, 2, 2, 2 } };
	message::Piece piece3{ { 0, 0, 0, 12, 7, 0, 0, 0, 0, 0, 0, 0, 20, 3, 4, 5 } };
	ReceivedPiece rp;
	rp.add_block(std::move(piece1));
	rp.add_block(std::move(piece2));
	rp.add_block(std::move(piece3));
	FileHandler fh({ "testfile", 1 }, { 0 }, 1, 21);
	fh.write_piece(rp, ".", 23);
}
