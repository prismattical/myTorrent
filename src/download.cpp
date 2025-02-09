#include "download.hpp"

#include "announce_list.hpp"
#include "bencode.hpp"
#include "config.hpp"
#include "download_strategy.hpp"
#include "expected.hpp"
#include "peer_connection.hpp"
#include "peer_message.hpp"
#include "tracker_connection.hpp"
#include "utils.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <netinet/in.h>
#include <optional>
#include <poll.h>
#include <span>
#include <stdexcept>
#include <string>
#include <sys/poll.h>
#include <tuple>
#include <utility>
#include <vector>

auto Peer::operator<=>(const Peer &other) const
{
	return ip <=> other.ip;
}

struct TrackerResponse {
	std::string failure_reason;
	std::string warning_message; // optional
	long long interval = 0;
	long long min_interval = 0; // optional;
	std::string tracker_id;
	long long complete = 0;
	long long incomplete = 0;
	std::vector<Peer> peers;
};

std::optional<TrackerResponse> parse_tracker_response(const std::string &response)
{
	TrackerResponse ret;

	const auto [status_code, status_message, headers, body] =
		utils::parse_http_response(response);

	// At first we check whether HTTP response was successful at all
	if ((status_code != 200 && status_code != 203) || body == "")
	{
		return std::nullopt;
	}

	auto resp_data = bencode::decode(body);

	const auto fr = utils::decode_optional_string(resp_data, "failure reason");
	// if we got a failure reason, then there is no need to continue. Just return what we got
	// I avoid using std::optional::value_or() in case some tracker decides to return an empty string as
	// a value of this field. I'm not sure this ever happens, but I choose to be safe
	if (fr.has_value())
	{
		ret.failure_reason = fr.value();
		return std::make_optional(std::move(ret));
	}

	// not all of these values are optional, but I prefer this syntax over try-catch blocks
	ret.warning_message =
		utils::decode_optional_string(resp_data, "warning message").value_or("");
	ret.interval = utils::decode_optional_int(resp_data, "interval").value_or(0);
	ret.min_interval = utils::decode_optional_int(resp_data, "min interval").value_or(0);
	ret.tracker_id = utils::decode_optional_string(resp_data, "tracker id").value_or("");
	ret.complete = utils::decode_optional_int(resp_data, "complete").value_or(0);
	ret.incomplete = utils::decode_optional_int(resp_data, "incomplete").value_or(0);

	if (std::holds_alternative<bencode::list>(resp_data["peers"]))
	{
		auto peer_list = std::get<bencode::list>(resp_data["peers"]);
		for (auto &peer : peer_list)
		{
			auto peer_dict = std::get<bencode::dict>(peer);
			const auto peer_id =
				utils::decode_optional_string(peer, "peer id").value_or("");
			// const auto peer_id = std::get<bencode::string>(peer_dict["peer id"]);
			const auto ip = std::get<bencode::string>(peer_dict["ip"]);
			const auto port =
				std::to_string(std::get<bencode::integer>(peer_dict["port"]));

			ret.peers.emplace_back(peer_id, ip, port);
		}
	}
	else if (std::holds_alternative<bencode::string>(resp_data["peers"]))
	{
		const std::string peer_string = std::get<bencode::string>(resp_data["peers"]);
		assert(peer_string.size() % (4 + 2) != 0 && "Malformed peers string received");

		for (size_t i = 0; i < peer_string.size(); i += (4 + 2))
		{
			uint32_t ip = 0;
			uint16_t port = 0;
			memcpy(&ip, peer_string.data() + i, sizeof ip);
			memcpy(&port, peer_string.data() + i + sizeof ip, sizeof port);

			ret.peers.emplace_back("", TCPClient::ntop(ip),
					       std::to_string(ntohs(port)));
		}
	}

	return std::make_optional(std::move(ret));
}

// Download ----------------------------------------------------------------------------

Download::Download(const std::string &path_to_torrent)
	: m_metainfo(path_to_torrent)
	, m_announce_list(std::move(m_metainfo.announce_list))
	, m_dl_strategy(std::make_unique<DownloadStrategySequential>(number_of_pieces()))
	, m_handshake(m_metainfo.info.get_sha1(), m_connection_id)
	, m_bitfield(number_of_pieces())
{
	create_download_layout();
	preallocate_files();
	check_layout();
}

void Download::create_download_layout()
{
	size_t index = 0;
	long long left_offset = 0;
	long long right_offset = 0;
	for (auto &fileinfo : m_metainfo.info.files)
	{
		std::set<size_t> needed_pieces;

		long long len = fileinfo.length + left_offset;
		while (len > 0)
		{
			needed_pieces.insert(index);
			len -= m_metainfo.info.piece_length;
			++index;
		}
		if (len < 0)
		{
			--index;
		}
		right_offset = -len;

		m_dl_layout.emplace_back(std::move(fileinfo), std::move(needed_pieces), left_offset,
					 right_offset);

		left_offset = m_metainfo.info.piece_length - right_offset;
	}

	m_last_piece_size = m_metainfo.info.piece_length - right_offset;

	m_dl_layout.back().mark_as_last_file();
}

void Download::check_layout()
{
	size_t i = 0;
	size_t j = 0;
	const auto piece_len = static_cast<size_t>(m_metainfo.info.piece_length);
	const std::filesystem::path fdir_path = m_metainfo.info.name;
	std::vector<uint8_t> piece(piece_len);
	while (j < m_dl_layout.size())
	{
		auto [is_full_piece, bytes_read] =
			m_dl_layout[j].read_piece(i, piece, fdir_path, piece_len);

		if (is_full_piece)
		{
			// we got the piece
			auto sha1 = utils::compute_sha1(piece);
			std::span<const uint8_t> supposed_sha1(
				reinterpret_cast<const uint8_t *>(m_metainfo.info.pieces.data()) +
					20 * i,
				20);

			std::cout << "Piece " << i << "/" << m_metainfo.info.pieces.size() / 20
				  << " is ";
			if (std::equal(sha1.begin(), sha1.end(), supposed_sha1.begin()))
			{
				std::cout << "already downloaded";
				m_bitfield.set_index(i, true);
			}
			else
			{
				std::cout << "not yet downloaded";
			}
			std::cout << '\n';

			++i;
			if (m_dl_layout[j].is_piece_part_of_file(i) == 1)
			{
				++j;
			}
		}
		else
		{
			++j;
		}
	}
}

void Download::preallocate_files()
{
	namespace fs = std::filesystem;
	const fs::path dl_root = config::get_path_to_downloads_dir() / m_metainfo.info.name;
	fs::create_directory(dl_root);
	const fs::path fdir_path = m_metainfo.info.name;
	for (const auto &file : m_dl_layout)
	{
		file.preallocate_file(fdir_path);
	}
}

size_t Download::number_of_pieces() const
{
	// this function exists only for readability purpose
	return m_metainfo.info.pieces.size() / 20;
}

void Download::handshake_cb(size_t /*index*/, std::span<const uint8_t> view)
{
	message::Handshake peer_hs(view);

	// todo: move validation code to handshake class
	if (peer_hs.is_valid(m_metainfo.info.get_sha1()))
	{
		// everything is fine
	}
	else
	{
		std::cerr << "Invalid handshake" << '\n';
		throw std::runtime_error("Connection terminated");
	}
}

void Download::keepalive_cb(size_t index, std::span<const uint8_t> view)
{
	// not implemented
}

void Download::choke_cb(size_t index, std::span<const uint8_t> /*view*/)
{
	auto &conn = m_peer_connections[index];
	conn.am_choking = true;
	conn.reset_request_queue();
	const auto ap = conn.assigned_pieces();
	for (auto index : ap)
	{
		m_dl_strategy->mark_as_discarded(index);
	}
}

void Download::unchoke_cb(size_t index, std::span<const uint8_t> /*view*/)
{
	auto &conn = m_peer_connections[index];
	conn.am_choking = false;

	const auto ind = m_dl_strategy->next_piece_to_dl(conn.peer_bitfield);
	if (!ind)
	{
		const auto err = ind.error();
		switch (err)
		{
		case DownloadStrategy::ReturnStatus::NO_PIECE_FOUND:
			conn.send_notinterested();
			return;
		case DownloadStrategy::ReturnStatus::DOWNLOAD_COMPLETED:
			throw std::runtime_error("Download completed");
		}
	}
	conn.send_interested();
	const size_t piece_length = ind == number_of_pieces() - 1 ? m_last_piece_size :
								    m_metainfo.info.piece_length;
	conn.create_requests_for_piece(ind.value(), piece_length);

	(void)conn.send_request();
}

void Download::interested_cb(size_t index, std::span<const uint8_t> view)
{
	// not implemented
}
void Download::notinterested_cb(size_t index, std::span<const uint8_t> view)
{
	// not implemented
}

void Download::have_cb(size_t index, std::span<const uint8_t> view)
{
	auto &conn = m_peer_connections[index];
	const message::Have have(view);
	conn.peer_bitfield.set_index(have.get_index(), true);

	if (conn.is_downloading())
	{
		// do nothing
	}
	else
	{
		if (m_dl_strategy->is_piece_missing(have))
		{
			conn.send_interested();
			if (conn.am_choking)
			{
				// wait for unchoke
				return;
			}
			const auto ind = m_dl_strategy->next_piece_to_dl(conn.peer_bitfield);
			if (!ind)
			{
				const auto err = ind.error();
				switch (err)
				{
				case DownloadStrategy::ReturnStatus::NO_PIECE_FOUND:
					conn.send_notinterested();
					return;
				case DownloadStrategy::ReturnStatus::DOWNLOAD_COMPLETED:
					throw std::runtime_error("Download completed");
				}
			}

			const size_t piece_length = ind == number_of_pieces() - 1 ?
							    m_last_piece_size :
							    m_metainfo.info.piece_length;
			conn.create_requests_for_piece(ind.value(), piece_length);

			(void)conn.send_request();
		}
	}
}

void Download::bitfield_cb(size_t index, std::span<const uint8_t> view)
{
	auto &conn = m_peer_connections[index];
	conn.peer_bitfield = message::Bitfield(view, m_bitfield.get_bf_size());

	if (!m_dl_strategy->have_missing_pieces(conn.peer_bitfield))
	{
		std::clog << "Peer does not have missing pieces" << '\n';
	}

	conn.send_interested();
}

void Download::request_cb(size_t index, std::span<const uint8_t> view)
{
	// not implemented
}

void Download::block_cb(size_t index, std::span<const uint8_t> view)
{
	auto &conn = m_peer_connections[index];

	int rc = conn.add_block();
	if (rc == -1)
	{
		std::cerr << "Block validation failed" << '\n';
		throw std::runtime_error("Connection terminated");
	}
	if (rc == 1)
	{
		ReceivedPiece piece = conn.get_received_piece();
		const std::string sha1 = piece.compute_sha1();
		const size_t ind = piece.get_index();

		const std::string sha1_expected =
			m_metainfo.info.pieces.substr(ind * utils::sha1_length, utils::sha1_length);
		if (sha1 == sha1_expected)
		{
			// on success
			for (const auto &fh : m_dl_layout)
			{
				int res = fh.is_piece_part_of_file(ind);
				if (res == 0)
				{
					fh.write_piece(piece, m_metainfo.info.name,
						       m_metainfo.info.piece_length);
				}
				if (res == 1)
				{
					break;
				}
			}
			std::clog << "Piece " << ind << " was received" << '\n';
		}
		else
		{
			m_dl_strategy->mark_as_discarded(ind);
			std::cerr << "Piece validation failed" << '\n';
			throw std::runtime_error("Connection terminated");
		}
	}

	if (conn.send_request() == 1)
	{
		auto ind = m_dl_strategy->next_piece_to_dl(conn.peer_bitfield);
		if (!ind)
		{
			const auto err = ind.error();
			switch (err)
			{
			case DownloadStrategy::ReturnStatus::NO_PIECE_FOUND:
				conn.send_notinterested();
				return;
			case DownloadStrategy::ReturnStatus::DOWNLOAD_COMPLETED:
				throw std::runtime_error("Download completed");
			}
		}
		else
		{
			const size_t piece_length = ind == number_of_pieces() - 1 ?
							    m_last_piece_size :
							    m_metainfo.info.piece_length;
			conn.create_requests_for_piece(ind.value(), piece_length);
			(void)conn.send_request();
		}
	}
}

void Download::cancel_cb(size_t index, std::span<const uint8_t> view)
{
	// not implemented
}
void Download::port_cb(size_t index, std::span<const uint8_t> view)
{
	// not implemented
}

void Download::peer_callback(const size_t index)
{
	const auto view = m_peer_connections[index].view_recv_message();
	if (view.size() <= 4)
	{
		std::clog << "Received KeepAlive from peer" << '\n';
		keepalive_cb(index, view);
	}
	else if (view.size() == 68 && view[0] == 19)
	{
		std::clog << "Received Handshake from peer" << '\n';
		handshake_cb(index, view);
	}
	else
	{
		switch (view[4])
		{
		case 0:

			std::clog << "Received Choke from peer" << '\n';
			choke_cb(index, view);
			break;

		case 1:
			// Unchoke

			std::clog << "Received Unchoke from peer" << '\n';
			unchoke_cb(index, view);
			break;

		case 2:
			// Interested
			std::clog << "Received Interested from peer" << '\n';
			interested_cb(index, view);
			break;

		case 3:
			// NotInterested
			std::clog << "Received NotInterested from peer" << '\n';
			notinterested_cb(index, view);
			break;
		case 4:
			// Have

			std::clog << "Received Have from peer" << '\n';
			have_cb(index, view);
			break;

		case 5:
			// Bitfield

			std::clog << "Received Bitfield from peer" << '\n';
			bitfield_cb(index, view);
			break;

		case 6:
			// Request
			std::clog << "Received Request from peer" << '\n';
			request_cb(index, view);
			break;

		case 7:
			// Piece
			std::clog << "Received Piece from peer" << '\n';
			block_cb(index, view);
			break;

		case 8:
			// Cancel
			std::clog << "Received Cancel from peer" << '\n';
			cancel_cb(index, view);
			break;
		case 9:
			// Port
			std::clog << "Received Port from peer" << '\n';
			port_cb(index, view);
			break;

		default:
			std::clog << "Received unknown message from peer" << '\n';
			throw std::runtime_error("Connection terminated");
			break;
		}
	}
}

void Download::proceed_peer(const size_t index)
{
	PeerConnection &peer_conn = m_peer_connections[index];
	struct pollfd &peer_pollfd = m_fds[index];

	if ((peer_pollfd.revents & POLLIN) != 0)
	{
		const int rc = peer_conn.recv();
		if (rc == 0)
		{
			peer_callback(index);
			if (peer_conn.should_wait_for_send())
			{
				peer_pollfd.events |= POLLOUT;
			}
		}
	}

	if ((peer_pollfd.revents & POLLOUT) != 0)
	{
		if (peer_conn.send() == 0)
		{
			std::cerr << "Successfully sent an entire msg to peer" << '\n';
			peer_pollfd.events &= ~POLLOUT;
		}
	}
	else if ((peer_pollfd.revents & (POLLERR | POLLHUP)) != 0)
	{
		throw std::runtime_error("Connection reset");
	}
}

void Download::add_peers_to_backlog(std::vector<Peer> &peer_addrs)
{
	for (Peer &peer : peer_addrs)
	{
		if (m_peers_in_use_or_banned.find(peer) == m_peers_in_use_or_banned.end())
		{
			m_peer_backlog.emplace(std::move(peer));
		}
	}
}

void Download::connect_to_peer(size_t index)
{
	while (!m_peer_backlog.empty())
	{
		auto it = m_peer_backlog.begin();
		try
		{
			m_peer_connections[index].connect(it->ip, it->port, m_handshake,
							  m_bitfield);
		} catch (const std::exception &ex)
		{
			// banned because failed to connect
			m_peers_in_use_or_banned.insert(*it);
			m_peer_backlog.extract(it);
			// try again until backlog empty
			continue;
		}
		// in use because we already connected
		m_peers_in_use_or_banned.insert(*it);
		m_peer_backlog.extract(it);
		m_fds[index] = { m_peer_connections[index].get_socket_fd(), (POLLIN | POLLOUT), 0 };
		break;
	}
}

void Download::tracker_callback()
{
	std::clog << "successfully reached tracker_callback()" << '\n';
	const auto span = m_tracker_connection.view_recv_message();
	const std::string str(reinterpret_cast<const char *>(span.data()), span.size());

	std::optional<TrackerResponse> resp = std::nullopt;
	try
	{
		resp = parse_tracker_response(str);
	} catch (const std::exception &ex)
	{
		resp = std::nullopt;
	}
	if (!resp.has_value())
	{
		std::cerr << "parse_tracker_response() failed" << '\n';
		if (m_announce_list.move_index_next() != 0)
		{
			m_announce_list.reset_index();
			m_fds.back() = { -1, 0, 0 };
			m_tracker_connection.set_timeout(m_timeout_on_failure);
		}
		throw std::runtime_error("tracker_callback() failed");
	}
	m_fds.back() = { -1, 0, 0 };
	m_tracker_connection.set_timeout(resp->interval);
	add_peers_to_backlog(resp->peers);
}

void Download::proceed_tracker()
{
	struct pollfd &tracker_pollfd = m_fds.back();

	if ((tracker_pollfd.revents & POLLIN) != 0)
	{
		const int rc = m_tracker_connection.recv();

		if (rc == 0)
		{
			tracker_callback();

			m_announce_list.move_current_tracker_to_top();
			m_announce_list.reset_index();

			tracker_pollfd = { -1, 0, 0 };
			return;
		}
	}

	if ((tracker_pollfd.revents & POLLOUT) != 0)
	{
		const int rc = m_tracker_connection.send();

		if (rc == 0)
		{
			tracker_pollfd.events = POLLIN;
		}
	}
	else if ((tracker_pollfd.revents & (POLLERR | POLLHUP)) != 0)
	{
		throw std::runtime_error("Connection reset");
	}
}

void Download::connect_to_tracker()
{
	const short ev = POLLOUT;
	const std::string info_hash = utils::convert_to_url(m_metainfo.info.get_sha1());
	TrackerRequestParams trp{};
	trp.info_hash = info_hash;
	trp.peer_id = m_connection_id;

	do
	{
		try
		{
			const auto [hostname, port] = m_announce_list.get_current_tracker();
			m_tracker_connection.connect(hostname, port, trp);
			m_fds.back() = { m_tracker_connection.get_socket_fd(), ev, 0 };
			return;
		} catch (const std::exception &ex)
		{
		}
	} while (m_announce_list.move_index_next() == 0);

	if (!has_peers_connected())
	{
		std::cerr << "Download is stalled due to tracker error" << '\n';
	}

	m_announce_list.reset_index();
	m_fds.back() = { -1, 0, 0 };
	m_tracker_connection.set_timeout(m_timeout_on_failure);
}

void Download::update_time_peer(size_t index)
{
	if (m_fds[index].fd != -1)
	{
		m_peer_connections[index].update_time();
	}
}

void Download::update_time_tracker()
{
	if (m_tracker_connection.update_time())
	{
		connect_to_tracker();
	}
}

void Download::poll()
{
	static constexpr int timeout = 1000; // 1 second

	const int rc = ::poll(m_fds.data(), m_fds.size(), timeout);
	if (rc > 0)
	{
		update_time_tracker();
		try
		{
			proceed_tracker();
		} catch (const std::exception &ex)
		{
			connect_to_tracker();
		}

		for (size_t i = 0; i < m_fds.size() - 1; ++i)
		{
			if (m_fds[i].fd != -1)
			{
				try
				{
					proceed_peer(i);
				} catch (const std::exception &ex)
				{
					std::cerr << "Peer disconected due to: " << ex.what()
						  << '\n';
					std::set<size_t> pieces =
						m_peer_connections[i].assigned_pieces();

					for (auto ind : pieces)
					{
						m_dl_strategy->mark_as_discarded(ind);
					}

					m_peer_connections[i].disconnect();
					m_fds[i] = { -1, 0, 0 };
				}
				update_time_peer(i);
			}
			else
			{
				connect_to_peer(i);
			}
		}
	}
	else if (rc == 0) // on timeout
	{
		update_time_tracker();

		for (size_t i = 0; i < m_fds.size() - 1; ++i)
		{
			if (m_fds[i].fd != -1)
			{
				update_time_peer(i);
			}
			else
			{
				connect_to_peer(i);
			}
		}
	}
	else
	{
		// on error
		throw std::runtime_error(std::string("poll(): ") + strerror(errno));
	}
}

void Download::start()
{
	m_tracker_connection.set_timeout(-1);
	while (true)
	{
		poll();
	}
}

bool Download::has_peers_connected() const
{
	return std::any_of(m_fds.begin(), m_fds.end(),
			   [](const pollfd &fd) { return fd.fd != -1; });
}

void Download::copy_metainfo_file_to_cache(const std::string &path_to_torrent)
{
	using std::filesystem::path;
	const path torrent_name = path(path_to_torrent).filename();
	const path path_to_destination =
		config::get_path_to_cache_dir().append(std::string(torrent_name));
	if (!exists(path_to_destination))
	{
		copy(path_to_torrent, path_to_destination);
	}
}
