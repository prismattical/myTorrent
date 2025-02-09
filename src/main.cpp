#include "config.hpp"
#include "download.hpp"

#include <iostream>
#include <memory>

int main(int argc, char *argv[])
{
	config::load_configs();
	config::create_downloads_dir();

	if (argc != 2)
	{
		std::cout << "Usage: myTorrent path_to_torrent" << '\n';
		return 1;
	}

	auto test_dl = std::make_unique<Download>(argv[1]);
	test_dl->start();

	return 0;
}
