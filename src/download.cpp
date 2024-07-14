#include "download.hpp"

#include "bencode.hpp"
#include "config.hpp"
#include "peer_message.hpp"
#include "piece.hpp"
#include "platform.hpp"
#include "socket.hpp"
#include "tracker_connection.hpp"
#include "utils.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

void Download::fill_peer_list()
{
	const auto [protocol, endpoint, path] = utils::parse_url(m_announce_url);
	const auto [domain_name, port] = utils::parse_endpoint(endpoint);
	TrackerConnection tr_conn(domain_name, port, m_connection_id);
	tr_conn.send_http_request(endpoint, path, m_info_hash, "8765");
	const std::string get_result = tr_conn.recv_http_request();
	const auto [return_code, body] = utils::parse_http_response(get_result);
	auto response_data = bencode::decode(body);
	if (std::holds_alternative<bencode::list>(response_data["peers"]))
	{
		const auto peer_list = std::get<bencode::list>(response_data["peers"]);
		for (const auto &peer : peer_list)
		{
			auto peer_dict = std::get<bencode::dict>(peer);
			const auto ip = std::get<bencode::string>(peer_dict["ip"]);
			const auto port =
				std::to_string(std::get<bencode::integer>(peer_dict["port"]));

			m_peers.push_back({ ip, port });
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

			m_peers.push_back({ Socket::ntop(ip), std::to_string(ntohs(port)) });
		}
	} else
	{
		throw std::runtime_error("HTTP response could not be decoded");
	}
	for (const auto &peer : m_peers)
	{
		std::cout << peer.first << ":" << peer.second << '\n';
	}
}

void Download::copy_torrent_to_cache(const std::string &path_to_torrent)
{
	const std::filesystem::path torrent_name =
		std::filesystem::path(path_to_torrent).filename();
	const std::filesystem::path path_to_destination =
		config::get_path_to_cache_dir().append(std::string(torrent_name));
	if (!std::filesystem::exists(path_to_destination))
	{
		std::filesystem::copy(path_to_torrent, path_to_destination);
	}
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
	m_info_hash = utils::sha1_to_url(utils::string_to_sha1(info));
	m_piece_length = std::get<bencode::integer_view>(info_dict_data["piece length"]);
	m_pieces = std::get<bencode::string_view>(info_dict_data["pieces"]);
	m_announce_url =
		std::string(std::get<bencode::string_view>(m_torrent_data_view["announce"]));
	m_filename = std::string(std::get<bencode::string_view>(info_dict_data["name"]));
	m_file_length = std::get<bencode::integer_view>(info_dict_data["length"]);
	m_connection_id = utils::generate_random_connection_id();

	this->fill_peer_list();
	// only once the client successfully gets the list of peers, we can copy .torrent to cache
	// otherwise an ill-formed .torrent file could be passed, that shouldn't be copied
	copy_torrent_to_cache(path_to_torrent);

	std::filesystem::path output_file = config::get_path_to_downloads_dir() / m_filename;

	if (!std::filesystem::exists(output_file) ||
	    std::filesystem::file_size(output_file) != static_cast<uintmax_t>(m_file_length))
	{
		utils::preallocate_file(output_file, m_file_length);
		m_bitfield = message::Bitfield(m_pieces.length() / 20);
	} else
	{
		std::ifstream output_file_stream(output_file, std::ios::in | std::ios::binary);
		m_bitfield = message::Bitfield(output_file_stream, m_piece_length, m_pieces);
	}

	Piece piece(m_piece_length);

}
