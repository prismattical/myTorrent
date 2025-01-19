#include "piece.hpp"

#include <cstdint>
#include <memory>
#include <openssl/evp.h>
#include <utility>

// ReceivedPiece -----------------------------------------------------------------------

void ReceivedPiece::add_block(message::Piece &&block)
{
	m_pieces.emplace_back(std::move(block));
}

void ReceivedPiece::clear()
{
	m_pieces.clear();
}

size_t ReceivedPiece::get_index() const
{
	return m_pieces.at(0).get_index();
}

std::string ReceivedPiece::compute_sha1() const
{
	static constexpr size_t sha1_length = 20;

	std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(),
								    EVP_MD_CTX_free);
	if (ctx == nullptr)
	{
		throw std::runtime_error("EVP_MD_CTX_new() has failed");
	}
	if (EVP_DigestInit_ex2(ctx.get(), EVP_sha1(), nullptr) == 0)
	{
		throw std::runtime_error("EVP_DigestInit_ex2() has failed");
	}

	for (const auto &piece : m_pieces)
	{
		const auto block = piece.get_data();
		if (EVP_DigestUpdate(ctx.get(), block.data(), block.size()) == 0)
		{
			throw std::runtime_error("EVP_DigestUpdate() has failed");
		}
	}
	std::string res;
	res.resize(sha1_length);

	if (EVP_DigestFinal_ex(ctx.get(), reinterpret_cast<uint8_t *>(res.data()), nullptr) == 0)
	{
		throw std::runtime_error("EVP_DigestFinal_ex() has failed");
	}

	return res;
}
