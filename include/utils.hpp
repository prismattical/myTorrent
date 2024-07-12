#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace utils
{

[[nodiscard]] std::vector<unsigned char> string_to_sha1(const std::string &input);

[[nodiscard]] std::string sha1_to_url(const std::vector<unsigned char> &input);

[[nodiscard]] std::string calculate_sha1(const std::vector<uint8_t> &piece);

// returns protocol, endpoint and path to resource in that order
[[nodiscard]] std::tuple<std::string, std::string, std::string> parse_url(const std::string &url);

// returns domain and port in that order
[[nodiscard]] std::tuple<std::string, std::string> parse_endpoint(const std::string &endpoint);

// returns HTTP response return code and HTTP body in that order
[[nodiscard]] std::tuple<int, std::string> parse_http_response(const std::string &responce);

[[nodiscard]] std::string generate_random_connection_id();
} // namespace utils
