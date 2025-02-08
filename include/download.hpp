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
#include <map>
#include <memory>
#include <poll.h>
#include <string>
#include <tuple>
#include <vector>

struct Peer {
	std::string peer_id;
	std::string ip;
	std::string port;

	auto operator<=>(const Peer &other) const;
};

class Download {
	std::array<uint8_t, utils::id_length> m_connection_id = utils::generate_connection_id();

	MetainfoFile m_metainfo;
	AnnounceList m_announce_list;
	std::unique_ptr<DownloadStrategy> m_dl_strategy;

	message::Handshake m_handshake;
	message::Bitfield m_bitfield;

	std::vector<FileHandler> m_dl_layout;
	std::set<Peer> m_peer_backlog;
	std::set<Peer> m_peers_in_use_or_banned;
	// std::vector<ReceivedPiece> m_pieces;

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

	void handshake_cb(size_t index, std::span<const uint8_t> view);
	void keepalive_cb(size_t index, std::span<const uint8_t> view);
	void choke_cb(size_t index, std::span<const uint8_t> view);
	void unchoke_cb(size_t index, std::span<const uint8_t> view);
	void interested_cb(size_t index, std::span<const uint8_t> view);
	void notinterested_cb(size_t index, std::span<const uint8_t> view);
	void have_cb(size_t index, std::span<const uint8_t> view);
	void bitfield_cb(size_t index, std::span<const uint8_t> view);
	void request_cb(size_t index, std::span<const uint8_t> view);
	void block_cb(size_t index, std::span<const uint8_t> view);
	void cancel_cb(size_t index, std::span<const uint8_t> view);
	void port_cb(size_t index, std::span<const uint8_t> view);

	void peer_callback(size_t index);
	void tracker_callback();

	void proceed_peer(size_t index);
	void proceed_tracker();

	void add_peers_to_backlog(std::vector<struct Peer> &peer_addrs);
	void connect_to_peer(size_t index);
	void connect_to_tracker();

	void update_time_peer(size_t index);
	void update_time_tracker();

	void poll();

public:
	explicit Download(const std::string &path_to_torrent);

	void start();
};
