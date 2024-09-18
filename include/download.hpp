#pragma once

#include "peer_connection.hpp"
#include "peer_message.hpp"
#include "tracker_connection.hpp"
#include "utils.hpp"

#include <poll.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
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


	message::Bitfield m_bitfield;

	// socket management

	static constexpr int m_timeout = 999999;
	static constexpr int m_max_peers = 50;

	std::vector<std::vector<std::string>> m_announce_urls;
	size_t m_last_tracker_tier;
	size_t m_last_tracker_tier_index;
	TrackerConnection m_tracker;

	std::array<PeerConnection, m_max_peers> m_peers;

	// the last one pollfd is tracker's pollfd
	std::array<struct pollfd, m_max_peers + 1> m_fds{ []() constexpr {
		std::array<struct pollfd, m_max_peers + 1> ret{};
		for (auto &pollfd : ret)
		{
			pollfd = { -1, 0, 0 };
		}
		return ret;
	}() };

	std::string m_connection_id = utils::generate_random_connection_id();

	void connect_to_tracker();
	void
	connect_to_new_peers(const std::vector<std::pair<std::string, std::string>> &peer_addrs);

	// return a vector of pairs <ip, port> that belong to possible peers
	// and an interval to wait between regular requests
	static std::tuple<std::vector<std::pair<std::string, std::string>>, long long>
	parse_tracker_response_http(const std::string &response);

public:
	Download(const std::string &path_to_torrent);

	void download();
};
