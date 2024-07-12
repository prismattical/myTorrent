#pragma once

#include <filesystem>
#include <sys/types.h>

namespace utils
{

void preallocate_file(const std::filesystem::path &path, long size);

} // namespace utils
