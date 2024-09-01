#pragma once

#include "peer_message.hpp"
#include "peer_pool.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

class Download {
	// source string for all views
	const std::string m_torrent_string;

	std::string m_info_hash;
	long long m_piece_length;
	std::string_view m_pieces;
	std::string m_filename;
	long long m_file_length;
	std::vector<uint8_t> m_info_hash_binary;

	std::string m_connection_id;

	std::vector<std::vector<std::string>> m_announce_urls;
	PeerPool m_peer_pool;

	message::Bitfield m_bitfield;

public:
	Download(const std::string &path_to_torrent);
};
