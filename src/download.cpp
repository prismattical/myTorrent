#include "download.hpp"

#include "bencode.hpp"
#include "config.hpp"
#include "peer_connection.hpp"
#include "peer_message.hpp"
#include "platform.hpp"

#include "utils.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/poll.h>
#include <vector>

std::tuple<std::vector<std::pair<std::string, std::string>>, long long>
Download::parse_tracker_response_http(const std::string &response)
{
	const auto [return_code, body] = utils::parse_http_response(response);
	auto response_data = bencode::decode(body);

	std::vector<std::pair<std::string, std::string>> peer_addresses;

	if (std::holds_alternative<bencode::list>(response_data["peers"]))
	{
		const auto peer_list = std::get<bencode::list>(response_data["peers"]);
		for (const auto &peer : peer_list)
		{
			auto peer_dict = std::get<bencode::dict>(peer);
			const auto ip = std::get<bencode::string>(peer_dict["ip"]);
			const auto port =
				std::to_string(std::get<bencode::integer>(peer_dict["port"]));

			peer_addresses.emplace_back(ip, port);
		}
	} else if (std::holds_alternative<bencode::string>(response_data["peers"]))
	{
		const std::string peer_string = std::get<bencode::string>(response_data["peers"]);
		if (peer_string.size() % (4 + 2) != 0)
		{
			throw std::runtime_error("Binary data peer string is ill-formed");
		}
		for (size_t i = 0; i < peer_string.size(); i += (4 + 2))
		{
			uint32_t ip;
			uint16_t port;
			// memcpy is safer than reinterpret_cast, though requires additional copy
			memcpy(&ip, peer_string.data() + i, sizeof ip);
			memcpy(&port, peer_string.data() + i + sizeof ip, sizeof port);

			peer_addresses.emplace_back(Socket::ntop(ip), std::to_string(ntohs(port)));
		}
	} else
	{
		throw std::runtime_error("HTTP response could not be decoded");
	}

	long long interval = std::get<bencode::integer>(response_data["interval"]);

	for (const auto &peer : peer_addresses)
	{
		std::cout << peer.first << ":" << peer.second << '\n';
	}

	return { peer_addresses, interval };
}

Download::Download(const std::string &path_to_torrent)
	: m_torrent_string{ [&]() {
		std::ifstream tor(path_to_torrent, std::ios_base::binary);
		std::string torrent_string;
		torrent_string.assign(std::istreambuf_iterator<char>(tor),
				      std::istreambuf_iterator<char>());

		return torrent_string;
	}() }
{
	bencode::data_view m_torrent_data_view = bencode::decode_view(m_torrent_string);
	bencode::data_view info_dict_data = m_torrent_data_view["info"];
	const std::string info = bencode::encode(info_dict_data);
	m_info_hash_binary = utils::string_to_sha1(info);
	m_info_hash = utils::sha1_to_url(m_info_hash_binary);
	m_piece_length = std::get<bencode::integer_view>(info_dict_data["piece length"]);
	m_pieces = std::get<bencode::string_view>(info_dict_data["pieces"]);
	m_filename = std::string(std::get<bencode::string_view>(info_dict_data["name"]));
	m_file_length = std::get<bencode::integer_view>(info_dict_data["length"]);

	// documentation for announce-list extension
	// http://bittorrent.org/beps/bep_0012.html
	try
	{
		const auto list_of_tiers =
			std::get<bencode::list_view>(m_torrent_data_view["announce-list"]);

		auto rd = std::random_device{};
		auto rng = std::default_random_engine{ rd() };

		for (size_t i = 0; i < list_of_tiers.size(); ++i)
		{
			m_announce_urls.emplace_back();
			const auto tier = std::get<bencode::list_view>(list_of_tiers[i]);
			for (const auto &url : tier)
			{
				m_announce_urls[i].emplace_back(
					std::get<bencode::string_view>(url));
			}
			// for whatever reason documentation says to shuffle each tier, so we shuffle
			std::shuffle(m_announce_urls[i].begin(), m_announce_urls[i].end(), rng);
		}

	} catch (const std::exception &ex)
	{
		// probably means the "announce-list" field is not present
		// so we grab one from "announce" field
		m_announce_urls.emplace_back();
		m_announce_urls[0].emplace_back(
			std::get<bencode::string_view>(m_torrent_data_view["announce"]));
	}

	// copy torrent to cache dir
	using namespace std::filesystem;
	const path torrent_name = path(path_to_torrent).filename();
	const path path_to_destination =
		config::get_path_to_cache_dir().append(std::string(torrent_name));
	if (!exists(path_to_destination))
	{
		copy(path_to_torrent, path_to_destination);
	}

	path output_file = config::get_path_to_downloads_dir() / m_filename;

	if (!exists(output_file) || file_size(output_file) != static_cast<uintmax_t>(m_file_length))
	{
		utils::preallocate_file(output_file, m_file_length);
		m_bitfield = message::Bitfield(m_pieces.length() / 20);
	} else
	{
		std::ifstream output_file_stream(output_file, std::ios::in | std::ios::binary);
		m_bitfield = message::Bitfield(output_file_stream, m_piece_length, m_pieces);
	}
}

void Download::connect_to_tracker()
{
	const auto announce_url = m_announce_urls[m_last_tracker_tier][m_last_tracker_tier_index];

	// TODO: rewrite parse_url function
	const auto [protocol, endpoint, _] = utils::parse_url(announce_url);
	const auto [domain_name, port] = utils::parse_endpoint(endpoint);

	m_tracker = TrackerConnection(domain_name, port, m_connection_id);
	m_tracker.connect();
	m_fds.back().fd = m_tracker.get_socket_fd();
	m_fds.back().events = POLLOUT;
}

void Download::connect_to_new_peers(
	const std::vector<std::pair<std::string, std::string>> &peer_addrs)
{
	for (size_t i = 0; i < m_peers.size(); ++i)
	{
		try
		{
			m_peers[i] = PeerConnection(peer_addrs[i].first, peer_addrs[i].second,
						    m_info_hash_binary, m_connection_id);
			m_fds[i].fd = m_peers[i].get_socket_fd();
			m_fds[i].events =
				POLLOUT; // wait until sock available for write to validate connection
		} catch (const std::runtime_error &ex)
		{
			// TODO: logging
			continue;
		}
	}
}

void Download::download()
{
	connect_to_tracker();
	while (true)
	{
		int rc = poll(m_fds.data(), m_fds.size(), m_timeout);
		if (rc > 0) // on success
		{
			// check tracker if connected
			if (m_fds.back().fd != -1)
			{
				if ((m_fds.back().revents & POLLOUT) != 0)
				{
					if (m_tracker.send_http("/announce", m_info_hash, "8765") ==
					    0)
					{ // on success change events to wait for repsonse
						m_fds.back().events = POLLIN;
					} else
					{ // on partial send poll until the next write is available
						m_fds.back().events = POLLOUT;
					}
				} else if ((m_fds.back().revents & POLLIN) != 0)
				{
					const std::string http_response =
						m_tracker.recv_http().value_or("-1");
					if (http_response != "-1")
					{ // on success turn off polling for tracker fd
						m_fds.back().fd = -1;

						const auto [peer_addrs, interval] =
							parse_tracker_response_http(http_response);
						// connect_to_new_peers(peer_addrs);
					} else // on partial read wait until the next read
					{
						m_fds.back().events = POLLIN;
					}
				}
			}
		} else if (rc == 0)
		{
			// on timeout
			throw std::runtime_error("poll() timeout expired");
		} else
		{
			// on error
			throw std::runtime_error(std::string("poll(): ") + strerror(errno));
		}
	}
}
