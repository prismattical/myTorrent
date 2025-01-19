#include "config.hpp"
#include "download.hpp"

#include <memory>

int main()
{
	config::load_configs();

	config::create_downloads_dir();

	auto test_dl = std::make_unique<Download>("./torrents/debian.torrent");
	test_dl->start();

	return 0;
}
