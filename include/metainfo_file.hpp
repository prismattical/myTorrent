#pragma once

#include "bencode.hpp"
#include "utils.hpp"

#include <filesystem>
#include <vector>

/**
 * @brief Struct that stores data from metainfo file
 *
 * It represents files list in info dictionary from the root of .torrent file
 */
struct FileInfo {
	std::filesystem::path path;
	long long length;
};

/**
 * @brief Struct that stores data from metainfo file
 * 
 * It represent info dictionary from the root of .torrent file
 * It treats single-file mode as multi-file mode with one file
 * It also computes and stores SHA1 hash of bencoded string of info dictionary
 */
struct InfoDict {
private:
	// this hash is neeeded in tracker requests and peer handshakes
	std::array<uint8_t, utils::sha1_length> m_sha1;

public:
	long long piece_length;
	std::string pieces;
	bool is_private; // optional, for private trackers
	std::filesystem::path name; // directory name
	std::vector<FileInfo> files;

	InfoDict() = default;
	InfoDict(bencode::data &source);

	[[nodiscard]] std::span<const uint8_t> get_sha1() const;
};

/**
 * @brief Struct that stores data from metainfo file
 * 
 * It represents the entire metainfo file
 */
struct MetainfoFile {
public:
	InfoDict info;
	std::string announce;
	std::vector<std::vector<std::string>> announce_list;
	long long creation_date;
	std::string comment;
	std::string created_by;

	MetainfoFile(const std::string &path_to_metainfo_file);
};
