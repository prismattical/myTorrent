#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace utils
{

inline constexpr size_t sha1_length = 20;
inline constexpr size_t id_length = 20;

/**
 * @brief Compute SHA1 from a continuous array of memory
 * 
 * SHA1 is a hash value that is 20 bytes long. This function takes a *single* chunk of data,
 * computes the hash, creates and returns an array that stores the value.
 * 
 * ! This function may segfault for unknown for me reason after several successful calls
 * ! during one execution
 */
[[nodiscard]] std::array<uint8_t, sha1_length> compute_sha1(std::span<const uint8_t> input);

/**
 * @brief Converts binary data to URL-encoded string
 * 
 * URL encoding is an encoding that stores any number, alphabetic character and symbols -._~
 * as a 1-byte ASCII value and any other value as three bytes: always a percent symbol %
 * followed by a hexadecimal value represented as a string. For example, a string "\n5"
 * will be encoded like "%0A5" 
 */
[[nodiscard]] std::string convert_to_url(std::span<const uint8_t> input);

/**
 * @brief Parses announce URL
 * 
 * @return protocol name, domain name and port that are specified in URL.
 * If no port is specified, returns default value 6969
 */
[[nodiscard]] std::tuple<std::string, std::string, std::string>
parse_announce_url(const std::string &url);

/**
 * @brief Parses HTTP response
 * 
 * This function should probably be rewritten to return all the information
 * specified in the response. Right now it only returns the essential for
 * this application parts.
 * 
 * @return HTTP return status code and body of the response.
 */
[[nodiscard]] std::tuple<int, std::string> parse_http_response(const std::string &responce);

/**
 * @brief Generates random connection id
 * 
 * The connection id consists of 20 random letters or numbers.
 * It is completely random and does not store any information about client.
 */
[[nodiscard]] std::array<uint8_t, id_length> generate_connection_id();
} // namespace utils
