#pragma once

#include <filesystem>

namespace config
{

void load_configs();

// located in root dir of executable
void create_cache_dir();
// located in root dir of executable
void create_downloads_dir();

[[nodiscard]] std::filesystem::path get_path_to_cache_dir();
[[nodiscard]] std::filesystem::path get_path_to_downloads_dir();

} // namespace config
