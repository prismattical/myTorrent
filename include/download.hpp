#pragma once

#include "announce_list.hpp"
#include "download_strategy.hpp"
#include "file_handler.hpp"
#include "metainfo_file.hpp"
#include "peer_connection.hpp"
#include "peer_message.hpp"
#include "piece.hpp"
#include "tracker_connection.hpp"
#include "utils.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

class Download {
	std::array<uint8_t, utils::id_length> m_connection_id = utils::generate_connection_id();

	MetainfoFile m_metainfo;
	AnnounceList m_announce_list;
	std::unique_ptr<DownloadStrategy> m_dl_strategy;

	message::Handshake m_handshake;
	message::Bitfield m_bitfield;

	std::vector<FileHandler> m_dl_layout;
	std::vector<ReceivedPiece> m_pieces;

	long long m_last_piece_size;

	static constexpr int m_max_peers = 10;

	static constexpr long long m_timeout_on_failure = 300;

	std::vector<PeerConnection> m_peer_connections{ m_max_peers };
	TrackerConnection m_tracker_connection;
	// m_fds.back() is tracker pollfd
	std::vector<struct pollfd> m_fds{ m_max_peers + 1, { -1, 0, 0 } };

	// general methods

	void create_download_layout();
	void check_layout();
	void preallocate_files();
	[[nodiscard]] size_t number_of_pieces() const;
	static void copy_metainfo_file_to_cache(const std::string &path_to_torrent);

	// async methods

	[[nodiscard]] bool has_peers_connected() const;

	static short connection_events(const TrackerConnection &connection);
	static short connection_events(const PeerConnection &connection);

	static std::tuple<std::vector<std::pair<std::string, std::string>>, long long>
	parse_tracker_response(const std::string &response);

	void peer_callback(size_t index);
	void tracker_callback();

	void proceed_peer(size_t index);
	void proceed_tracker();

	void
	connect_to_new_peers(const std::vector<std::pair<std::string, std::string>> &peer_addrs);
	void connect_to_tracker();

	void update_time_peer(size_t index);
	void update_time_tracker();

	void poll();

public:
	Download(const std::string &path_to_torrent);

	void start();

	class AllTrackersNotRespondingError : public std::runtime_error {
	public:
		AllTrackersNotRespondingError(const std::string &msg);
	};
};
