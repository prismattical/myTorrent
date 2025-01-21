#include "metainfo_file.hpp"

#include <fstream>
#include <optional>
#include <random>
#include <string>

// InfoDict ----------------------------------------------------------------------------

std::optional<std::string> decode_optional_string(bencode::data &source, const std::string &key)
{
	try
	{
		return std::string(std::get<bencode::string>(source[key]));
	} catch (const std::exception &ex)
	{
		return std::nullopt;
	}
}

std::optional<long long> decode_optional_int(bencode::data &source, const std::string &key)
{
	try
	{
		return std::get<bencode::integer>(source[key]);
	} catch (const std::exception &ex)
	{
		return std::nullopt;
	}
}

std::optional<bencode::list> decode_optional_list_view(bencode::data &source, const std::string &key)
{
	try
	{
		return std::get<bencode::list>(source[key]);
	} catch (const std::exception &ex)
	{
		return std::nullopt;
	}
}

InfoDict::InfoDict(bencode::data &source)
{
	// compute and store SHA1 hash of a bencoded string containing info dictionary
	// this hash is neeeded in tracker requests and peer handshakes
	std::string info = bencode::encode(source);

	m_sha1 = utils::compute_sha1(
		{ reinterpret_cast<const uint8_t *>(info.data()), info.size() });

	piece_length = std::get<bencode::integer_view>(source["piece length"]);
	pieces = std::get<bencode::string>(source["pieces"]);
	// private trackers are not supported (yet)
	// so this variable is unused
	is_private = decode_optional_int(source, "private").value_or(0) == 1;

	bencode::list files_list =
		decode_optional_list_view(source, "files").value_or(bencode::list());
	// single file mode is treated as multifile, but with a single file
	if (files_list.empty())
	{
		name = ".";
		const auto path = "./" + std::get<bencode::string>(source["name"]);
		const auto length = std::get<bencode::integer_view>(source["length"]);

		files.emplace_back(path, length);
	} else
	{
		name = std::get<bencode::string>(source["name"]);
		for (auto &file : files_list)
		{
			auto path_list_temp = file["path"];
			auto path_list = std::get<bencode::list>(path_list_temp);
			const auto length = std::get<bencode::integer_view>(file["length"]);
			std::filesystem::path path = ".";
			for (auto &part : path_list)
			{
				auto part_str = std::get<bencode::string>(part);
				path /= part_str;
			}

			files.emplace_back(path, length);
		}
	}
}

std::span<const uint8_t> InfoDict::get_sha1() const
{
	return m_sha1;
}

// MetainfoFile ------------------------------------------------------------------------

MetainfoFile::MetainfoFile(const std::string &path_to_metainfo_file)
{
	std::ifstream tor(path_to_metainfo_file, std::ios_base::binary);
	std::string torrent_string;
	torrent_string.assign(std::istreambuf_iterator<char>(tor),
			      std::istreambuf_iterator<char>());

	bencode::data torrent_data = bencode::decode(torrent_string);

	creation_date = decode_optional_int(torrent_data, "creation date").value_or(-1);
	comment = decode_optional_string(torrent_data, "comment").value_or("");
	created_by = decode_optional_string(torrent_data, "created by").value_or("");

	announce = std::get<bencode::string>(torrent_data["announce"]);

	bencode::list list_of_tiers =
		decode_optional_list_view(torrent_data, "announce-list").value_or(bencode::list());

	if (list_of_tiers.empty())
	{
		// if announce-list field is not present we emplace the only URL from announce field
		announce_list.emplace_back();
		announce_list[0].emplace_back(announce);
	} else
	{
		auto list_of_tiers = std::get<bencode::list>(torrent_data["announce-list"]);

		auto rd = std::random_device{};
		auto rng = std::default_random_engine{ rd() };

		for (size_t i = 0; i < list_of_tiers.size(); ++i)
		{
			announce_list.emplace_back();
			auto tier = std::get<bencode::list>(list_of_tiers[i]);
			for (auto &url : tier)
			{
				announce_list[i].emplace_back(std::get<bencode::string>(url));
			}
			// for whatever reason documentation says to shuffle each tier, so we shuffle
			std::shuffle(announce_list[i].begin(), announce_list[i].end(), rng);
		}
	}

	bencode::data info_data_view = torrent_data["info"];

	info = InfoDict(info_data_view);
}
