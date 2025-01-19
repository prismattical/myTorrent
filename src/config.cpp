#include "config.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace config
{

static std::filesystem::path g_path_to_app_root;

static std::filesystem::path g_path_to_cache_dir;
static std::filesystem::path g_path_to_downloads_dir;

void load_configs()
{
	g_path_to_app_root = std::filesystem::canonical("/proc/self/exe");
	g_path_to_app_root.remove_filename();

	std::filesystem::path path_to_config = g_path_to_app_root / "configs.conf";
	if (!std::filesystem::exists(path_to_config))
	{
		return;
	}
	
	std::ifstream file(path_to_config);

	for (std::string line; std::getline(file, line);)
	{
		std::istringstream line_stream(line);
		std::string key;
		std::string value;

		char ch;
		while (line_stream.get(ch) && ch != '=')
		{
			if (isblank(ch) == 0)
			{
				key += ch;
			}
		}
		if (ch != '=')
		{
			continue;
		}
		line_stream >> value;

		std::cout << key << ch << value << '\n';
	}
}

void create_cache_dir()
{
	g_path_to_cache_dir = g_path_to_app_root / "cache";
	std::filesystem::create_directory(g_path_to_cache_dir);
}

void create_downloads_dir()
{
	g_path_to_downloads_dir = g_path_to_app_root / "downloads";
	std::filesystem::create_directory(g_path_to_downloads_dir);
}

std::filesystem::path get_path_to_cache_dir()
{
	return g_path_to_cache_dir;
}

std::filesystem::path get_path_to_downloads_dir()
{
	return g_path_to_downloads_dir;
}

} // namespace config
