#include "config.hpp"
#include "download.hpp"

int main()
{
	config::load_configs();
	config::create_cache_dir();
	config::create_downloads_dir();

	Download test_dl("torrents/debian.torrent");
	test_dl.download();

	return 0;
}
