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
#include <fstream>
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

struct Peer {
	std::string peer_id;
	std::string ip;
	std::string port;
};

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
		const auto peer_list = std::get<bencode::list>(resp_data["peers"]);
		for (const auto &peer : peer_list)
		{
			auto peer_dict = std::get<bencode::dict>(peer);
			const auto peer_id = std::get<bencode::string>(peer_dict["peer id"]);
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
	, m_pieces(number_of_pieces())
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

short Download::connection_events(const TrackerConnection &connection)
{
	// this function exists only for readability purpose
	return POLLIN | (connection.should_wait_for_send() ? POLLOUT : 0);
}

short Download::connection_events(const PeerConnection &connection)
{
	// this function exists only for readability purpose
	return POLLIN | (connection.should_wait_for_send() ? POLLOUT : 0);
}

void Download::connect_to_new_peers(const std::vector<Peer> &peer_addrs)
{
	// this ugly function takes all the new peer ip+port pairs
	// iterates over all existing slots for file descriptors in m_fds vector and
	// if the slot is empty (i.e. it equates to -1), then it tries to connect
	// if during the try an exception is thrown, it retries with the next ip+port pair
	// until either it succeeds, or we run out of pairs

	// I **believe** this function can not become more readable
	// at least AI couldn't come up with anything clever
	// but if there is a better solution without changing overall architecture,
	// I would like to know it

	static size_t times_called = 0;
	std::clog << "connect_to_new_peers() called " << ++times_called << " times" << '\n';

	auto it = peer_addrs.begin();
	for (size_t i = 0; i < m_peer_connections.size() - 1 && it != peer_addrs.end(); ++i)
	{
		if (m_fds[i].fd == -1)
		{
			while (it != peer_addrs.end())
			{
				auto &this_conn = m_peer_connections[i];
				try
				{
					this_conn = PeerConnection(it->ip, it->port, m_handshake,
								   m_bitfield);

				} catch (const std::exception &ex)
				{
					++it;
					continue;
				}
				++it;
				m_fds[i] = { this_conn.get_socket_fd(),
					     connection_events(this_conn), 0 };

				break;
			}
		}
	}
	const size_t peers_connected = std::count_if(
		m_fds.begin(), m_fds.end(), [](const pollfd &pfd) { return pfd.fd != -1; });
	std::clog << "Connected to " << peers_connected << " peers" << '\n';
}

void Download::tracker_callback()
{
	static size_t times_called = 0;
	std::clog << "tracker_callback() called " << ++times_called << " times" << '\n';

	const auto span = m_tracker_connection.view_recv_message();
	const std::string str(reinterpret_cast<const char *>(span.data()), span.size());
	try
	{
		// todo: rewrite this part
		const auto resp = parse_tracker_response(str);
		if (!resp.has_value())
		{
			throw std::runtime_error("parse_tracker_response() failed");
		}
		m_tracker_connection = {};
		m_fds.back() = { -1, 0, 0 };
		m_tracker_connection.set_timeout(resp->interval);
		connect_to_new_peers(resp->peers);
	} catch (const std::exception &ex)
	{
		// if failed to do the callback,
		// then try to move to next tracker
		if (m_announce_list.move_index_next() != 0)
		{
			// if it was already last tracker, close the connection
			// and set the default timeout and return from the function
			m_announce_list.reset_index();
			m_tracker_connection = {};
			m_fds.back() = { -1, 0, 0 };
			m_tracker_connection.set_timeout(m_timeout_on_failure);
			if (!has_peers_connected())
			{
				// but if we currently are not connected to any peers, then throw instead
				throw std::runtime_error(
					"Download is stalled due to tracker error");
			}
		}
		else
		{
			// if it was not the last tracker, then we close the connection and set timeout to 1 sec
			// after 1 second the timeout will expire and function connect_to_tracker will be called again
			m_tracker_connection = {};
			m_fds.back() = { -1, 0, 0 };
			m_tracker_connection.set_timeout(1);
		}
	}
}

void Download::peer_callback(const size_t index)
{
	auto &conn = m_peer_connections[index];
	const auto request_new_pieces = [this, &conn, index]() {
		const size_t ind = m_dl_strategy->next_piece_to_dl(conn.peer_bitfield);
		if (ind == number_of_pieces())
		{
			conn.send_notinterested();
			return;
		}
		size_t piece_length = ind == number_of_pieces() - 1 ? m_last_piece_size :
								      m_metainfo.info.piece_length;
		conn.create_requests_for_piece(ind, piece_length);
		conn.send_initial_requests();
		std::clog << "Requesting piece " << ind << " from peer " << index << '\n';
	};

	const auto view = conn.view_recv_message();
	if (view.size() <= 4)
	{
		//  KeepAlive
		std::clog << "Received KeepAlive from peer" << '\n';
	}
	else if (view.size() == 68)
	{
		// Handshake
		std::clog << "Received Handshake from peer" << '\n';
		// todo: handle
	}
	else
	{
		switch (view[4])
		{
		case 0:
			// Choke
			{
				std::clog << "Received Choke from peer" << '\n';

				conn.am_choking = true;
				conn.cancel_requests_on_choke();

				break;
			}
		case 1:
			// Unchoke
			{
				std::clog << "Received Unchoke from peer" << '\n';

				conn.am_choking = false;

				if (!conn.is_downloading())
				{
					request_new_pieces();
				}

				break;
			}
		case 2:
			// Interested
			std::clog << "Received Interested from peer" << '\n';

			// ! Ignored
			break;

		case 3:
			// NotInterested
			std::clog << "Received NotInterested from peer" << '\n';

			// ! Ignored
			break;
		case 4:
			// Have
			{
				std::clog << "Received Have from peer" << '\n';

				message::Have have(view);
				conn.peer_bitfield.set_index(have.get_index(), true);
				if (m_dl_strategy->is_piece_missing(have))
				{
					conn.send_interested();
				}
				if (!conn.am_choking && !conn.is_downloading())
				{
					request_new_pieces();
				}
				break;
			}
		case 5:
			// Bitfield
			{
				std::clog << "Received Bitfield from peer" << '\n';

				conn.peer_bitfield = { view, m_bitfield.get_bitfield_length() };
				if (m_dl_strategy->have_missing_pieces(conn.peer_bitfield))
				{
					conn.send_interested();
				}
				if (!conn.am_choking
				    // && !conn.is_downloading()
				)
				{
					request_new_pieces();
				}
				break;
			}
		case 6:
			// Request
			std::clog << "Received Request from peer" << '\n';

			// ! Ignored
			break;

		case 7: {
			// Piece
			std::clog << "Received Piece from peer" << '\n';

			std::vector<uint8_t> msg = conn.get_recv_message();

			message::Piece block(std::move(msg));
			const uint32_t ind = block.get_index();
			m_pieces[ind].add_block(std::move(block));
			std::clog << "Received block " << m_pieces[ind].m_pieces.size() - 1
				  << " for piece " << ind << " meanwhile conn.m_rq_current "
				  << conn.m_rq_current << '\n';

			if (conn.send_requests() != 0) // last block received
			{
				std::ofstream fout("testout");

				for (auto &piece : m_pieces[ind].m_pieces)
				{
					fout.write(reinterpret_cast<const char *>(
							   piece.get_data().data()),
						   piece.get_data().size());
				}
				// check SHA1
				const std::string sha1_received = m_pieces[ind].compute_sha1();
				const std::string sha1_expected = m_metainfo.info.pieces.substr(
					ind * utils::sha1_length, utils::sha1_length);

				assert(sha1_received.size() == 20);

				if (sha1_received != sha1_expected)
				{ // on failed SHA1 check
					m_dl_strategy->mark_as_discarded(ind);
					m_pieces[ind].clear();
					std::clog << "Piece " << ind
						  << " discarded due to wrong SHA1" << '\n';
				}
				else // on successful SHA1 check
				{
					// write piece
					for (const auto &fh : m_dl_layout)
					{
						int res = fh.is_piece_part_of_file(ind);
						if (res == 0)
						{
							fh.write_piece(
								m_pieces[ind], m_metainfo.info.name,
								m_metainfo.info.piece_length);
							std::clog << "Piece " << ind
								  << " was written into the file"
								  << '\n';
						}
						if (res == 1)
						{
							break;
						}
					}
				}
				if (!conn.am_choking)
				{
					request_new_pieces();
				}
			}
			else
			{
				// probably do nothing idk
			}
			break;
		}
		case 8:
			// Cancel
			std::clog << "Received Cancel from peer" << '\n';

			// ! Ignored
			break;
		case 9:
			// Port
			std::clog << "Received Port from peer" << '\n';

			// ! Ignored
			break;

		default:
			std::clog << "Received unknown message from peer" << '\n';

			break;
		}
	}
}

void Download::proceed_peer(const size_t index)
{
	if ((m_fds[index].revents & POLLIN) != 0)
	{
		const int rc = m_peer_connections[index].recv();
		if (rc == 0)
		{
			peer_callback(index);
		}
	}

	if ((m_fds[index].revents & POLLOUT) != 0)
	{
		(void)m_peer_connections[index].send();
	}
	else if ((m_fds[index].revents & (POLLERR | POLLHUP)) != 0)
	{
		throw std::runtime_error("Connection reset");
	}

	m_fds[index].events = connection_events(m_peer_connections[index]);
}

void Download::proceed_tracker()
{
	if ((m_fds.back().revents & POLLIN) != 0)
	{
		const int rc = m_tracker_connection.recv();

		if (rc == 0)
		{
			tracker_callback();

			m_announce_list.move_current_tracker_to_top();
			m_announce_list.reset_index();

			m_fds.back() = { -1, 0, 0 };
			return;
		}
	}

	if ((m_fds.back().revents & POLLOUT) != 0)
	{
		(void)m_tracker_connection.send();
	}
	else if ((m_fds.back().revents & (POLLERR | POLLHUP)) != 0)
	{
		throw std::runtime_error("Connection reset");
	}

	m_fds.back().events = connection_events(m_tracker_connection);
}

void Download::connect_to_tracker()
{
	std::clog << "connect_to_tracker() was called" << '\n';

	const auto connect = [this](const std::string &hostname, const std::string &port) {
		const std::string info_hash = utils::convert_to_url(m_metainfo.info.get_sha1());
		m_tracker_connection =
			TrackerConnection(hostname, port, m_connection_id, info_hash);
		const short events = connection_events(m_tracker_connection);
		m_fds.back() = { m_tracker_connection.get_socket_fd(), events, 0 };
	};

	while (true)
	{
		try
		{
			const auto [hostname, port] = m_announce_list.get_current_tracker();
			connect(hostname, port);
			break;
		} catch (const std::exception &ex)
		{
			if (m_announce_list.move_index_next() != 0)
			{
				// if can't connect to any tracker url
				// then close the connection and set default timeout
				m_announce_list.reset_index();
				m_tracker_connection = {};
				m_fds.back() = { -1, 0, 0 };
				m_tracker_connection.set_timeout(m_timeout_on_failure);
				if (!has_peers_connected())
				{
					// if also aren't connected to any peer, then throw
					throw std::runtime_error(
						"Download is stalled due to tracker error");
				}
				// if connected to peers, then just exit the function
				break;
			}
		}
	}
}

void Download::update_time_peer(size_t index)
{
	m_peer_connections[index].update_time();
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
		proceed_tracker();
		update_time_tracker();

		for (size_t i = 0; i < m_fds.size() - 1; ++i)
		{
			try
			{
				proceed_peer(i);
			} catch (const std::exception &ex)
			{
				// TODO: handle somehow
				std::cerr << "Peer disconected due to: " << ex.what() << '\n';
				m_fds[i] = { -1, 0, 0 };
				m_peer_connections[i] = {};
			}
			update_time_peer(i);
		}
	}
	else if (rc == 0) // on timeout
	{
		update_time_tracker();

		for (size_t i = 0; i < m_fds.size() - 1; ++i)
		{
			update_time_peer(i);
		}
	}
	else
	{
		// on error
		throw std::runtime_error(std::string("poll(): ") + strerror(errno));
	}
}

bool Download::has_peers_connected() const
{
	return std::any_of(m_fds.begin(), m_fds.end(),
			   [](const pollfd &fd) { return fd.fd != -1; });
}

void Download::start()
{
	connect_to_tracker();
	while (true)
	{
		poll();
	}
}

void Download::copy_metainfo_file_to_cache(const std::string &path_to_torrent)
{
	using namespace std::filesystem;
	const path torrent_name = path(path_to_torrent).filename();
	const path path_to_destination =
		config::get_path_to_cache_dir().append(std::string(torrent_name));
	if (!exists(path_to_destination))
	{
		copy(path_to_torrent, path_to_destination);
	}
}

Download::AllTrackersNotRespondingError::AllTrackersNotRespondingError(const std::string &msg)
	: std::runtime_error(msg)
{
}
