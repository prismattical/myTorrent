#include "utils.hpp"

#include <openssl/evp.h>

#include <cctype>
#include <iomanip>
#include <iostream>
#include <random>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>

namespace utils
{

std::array<uint8_t, sha1_length> compute_sha1(std::span<const uint8_t> input)

{
	std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(),
								    EVP_MD_CTX_free);
	if (ctx == nullptr)
	{
		throw std::runtime_error("EVP_MD_CTX_new() has failed");
	}
	if (EVP_DigestInit_ex2(ctx.get(), EVP_sha1(), nullptr) == 0)
	{
		throw std::runtime_error("EVP_DigestInit_ex2() has failed");
	}
	if (EVP_DigestUpdate(ctx.get(), input.data(), input.size()) == 0)
	{
		throw std::runtime_error("EVP_DigestUpdate() has failed");
	}
	std::array<uint8_t, sha1_length> hash;

	if (EVP_DigestFinal_ex(ctx.get(), hash.data(), nullptr) == 0)
	{
		throw std::runtime_error("EVP_DigestFinal_ex() has failed");
	}

	return hash;
}

std::string convert_to_url(std::span<const uint8_t> input)
{
	std::string ret;
	for (const unsigned char ch : input)
	{
		if (isalnum(ch) != 0 || ch == '-' || ch == '_' || ch == '.' || ch == '~')
		{
			// compiler warns about narrowing conversion, but it can't happen
			// because if we take this branch of if statement, ch < 128
			ret += ch;
		} else
		{
			std::stringstream ss;
			ss << '%' << std::setw(2) << std::setfill('0') << std::hex
			   << static_cast<unsigned int>(ch);
			ret.append(ss.str());
		}
	}
	return ret;
}

std::tuple<std::string, std::string, std::string> parse_announce_url(const std::string &url)
{
	const std::regex regex(R"(^([a-z]+)://([^:]+):(\d+)?(/.*)?$)");

	std::smatch match;
	if (!std::regex_match(url, match, regex))
	{
		throw std::runtime_error("parse_url(): regex_match() error");
	}

	std::string protocol = match[1].str();
	std::string endpoint = match[2].str();
	std::string port = match.size() > 3 ? match[3].str() : "6969";

	return std::make_tuple(protocol, endpoint, port);
}

std::tuple<int, std::string> parse_http_response(const std::string &responce)
{
	std::stringstream text(responce);

	std::string line;

	// parse first line
	if (!std::getline(text, line))
	{
		throw std::runtime_error("Empty HTTP response");
	}
	std::string http_version;
	int return_code;
	std::string explanation;
	std::stringstream first_line(line);
	first_line >> http_version >> return_code >> explanation;

	// find start of the body
	while (std::getline(text, line))
	{
		// found the empty line that divides headers from body
		if (line == "\r")
		{
			break;
		}
	}

	std::string body;
	char ch;

	while (text.get(ch))
	{
		body.push_back(ch);
	}

	if (body.empty())
	{
		throw std::runtime_error("Empty body in HTTP response");
	}

	return { return_code, body };
}

std::array<uint8_t, id_length> generate_connection_id()
{
	std::random_device rd;
	std::mt19937 mt(rd());
	std::uniform_int_distribution<char> dist_num(48, 57);
	std::uniform_int_distribution<char> dist_low(65, 90);
	std::uniform_int_distribution<char> dist_up(97, 122);

	std::uniform_int_distribution<int> dist_choice(0, 2);

	std::array<uint8_t, id_length> ret;

	for (unsigned char &ch : ret)
	{
		const int c = dist_choice(mt);
		switch (c)
		{
		case 0:
			ch = dist_num(mt);
			break;
		case 1:
			ch = dist_low(mt);
			break;
		case 2:
			ch = dist_up(mt);
			break;
		default:
			break;
		}
	}
	return ret;
}

} // namespace utils
