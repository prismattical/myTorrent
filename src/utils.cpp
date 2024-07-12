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
#include <vector>

namespace utils
{

std::vector<unsigned char> string_to_sha1(const std::string &input)
{
	EVP_MD_CTX *ctx = EVP_MD_CTX_create();
	EVP_MD_CTX_init(ctx);
	EVP_DigestInit_ex(ctx, EVP_sha1(), nullptr);

	EVP_DigestUpdate(ctx, input.data(), input.size());

	const unsigned int digest_size = EVP_MD_size(EVP_sha1());
	std::vector<unsigned char> hash(digest_size);

	unsigned int final_size;
	EVP_DigestFinal_ex(ctx, hash.data(), &final_size);

	EVP_MD_CTX_destroy(ctx);

	hash.resize(final_size);

	return hash;
}

std::string calculate_sha1(const std::vector<uint8_t> &piece)
{
	EVP_MD_CTX *ctx = EVP_MD_CTX_create();
	EVP_MD_CTX_init(ctx);
	EVP_DigestInit_ex(ctx, EVP_sha1(), nullptr);

	EVP_DigestUpdate(ctx, piece.data(), piece.size());

	const unsigned int digest_size = EVP_MD_size(EVP_sha1());
	std::vector<unsigned char> hash(digest_size);

	unsigned int final_size;
	EVP_DigestFinal_ex(ctx, hash.data(), &final_size);

	EVP_MD_CTX_destroy(ctx);

	hash.resize(final_size);

	return std::string{ hash.begin(), hash.end() };
}

std::string sha1_to_url(const std::vector<unsigned char> &input)
{
	std::string ret;
	for (const unsigned char ch : input)
	{
		if (isalnum(ch) != 0 || ch == '-' || ch == '_' || ch == '.' || ch == '~')
		{
			ret += ch;
		} else
		{
			std::stringstream ss;
			ss << '%' << std::setw(2) << std::setfill('0') << std::hex
			   << (unsigned int)ch;
			ret.append(ss.str());
		}
	}
	return ret;
}

std::tuple<std::string, std::string, std::string> parse_url(const std::string &url)
{
	const std::regex pattern(R"(^([a-z]+)://([^/]*)(/.*)?)");

	try
	{
		std::smatch match;
		if (std::regex_search(url, match, pattern))
		{
			const std::string protocol = match[1].str();
			const std::string domain = match[2].str();
			const std::string path = match[3].str().empty() ? "/" : match[3].str();
			return std::make_tuple(protocol, domain, path);
		}
		throw std::runtime_error("Invalid URL format");

	} catch (const std::regex_error &e)
	{
		throw std::runtime_error("parse_url(): " + std::string(e.what()));
	}
}

std::tuple<std::string, std::string> parse_endpoint(const std::string &endpoint)
{
	const std::regex pattern("([^:]+):(\\d+)");

	try
	{
		std::smatch match;
		if (std::regex_search(endpoint, match, pattern))
		{
			return std::make_tuple(match[1].str(), match[2].str());
		}
		return std::make_tuple(endpoint, "6969");

		throw std::runtime_error("Invalid endpoint format");

	} catch (const std::regex_error &e)
	{
		throw std::runtime_error("parse_endpoint(): " + std::string(e.what()));
	}
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

std::string generate_random_connection_id()
{
	std::random_device rd;
	std::mt19937 mt(rd());
	std::uniform_int_distribution<char> dist_num(48, 57);
	std::uniform_int_distribution<char> dist_low(65, 90);
	std::uniform_int_distribution<char> dist_up(97, 122);

	std::uniform_int_distribution<int> dist_choice(0, 2);

	std::string ret /* = "-mT0001-"*/;

	for (int i = 0; i < 20; ++i)
	{
		const int c = dist_choice(mt);
		switch (c)
		{
		case 0:
			ret += dist_num(mt);
			break;
		case 1:
			ret += dist_low(mt);
			break;
		case 2:
			ret += dist_up(mt);
			break;
		default:
			break;
		}
	}
	return ret;
}

} // namespace utils
