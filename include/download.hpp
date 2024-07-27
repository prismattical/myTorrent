#pragma once

#include "peer_message.hpp"

#include <string>
#include <string_view>
#include <vector>

class Download {
	// source string for all views
	const std::string m_torrent_string;

	std::string m_info_hash;
	long long m_piece_length;
	std::string_view m_pieces;
	std::string m_announce_url;
	std::string m_filename;
	long long m_file_length;
	std::string m_connection_id;

	std::vector<unsigned char> m_info_hash_binary;

	std::vector<std::pair<std::string, std::string>> m_peers;

	message::Bitfield m_bitfield;

	void fill_peer_list();
	static void copy_torrent_to_cache(const std::string &path_to_torrent);

public:
	Download(const std::string &path_to_torrent);
};
