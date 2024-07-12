#include "platform.hpp"

#include <cstdio>
// assume Linux
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/types.h>
#include <unistd.h>

namespace utils
{

void preallocate_file(const std::filesystem::path &path, const long size)
{
	// assume Linux
	int fd = open(path.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd == -1)
	{
		throw std::runtime_error(std::string("preallocate_file(): open(): ") +
					 strerror(errno));
	}
	if (posix_fallocate(fd, 0, size) != 0)
	{
		int errno_save = errno;
		close(fd);
		throw std::runtime_error(std::string("preallocate_file(): posix_fallocate(): ") +
					 strerror(errno_save));
	}
	fsync(fd);
	close(fd);
}

} // namespace utils
