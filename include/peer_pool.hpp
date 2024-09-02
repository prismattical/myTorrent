#pragma once

#include "peer_connection.hpp"
#include "tracker_connection.hpp"
#include "utils.hpp"

#include <poll.h>

#include <array>
#include <string>
#include <vector>

class PeerPool {
private:
	static constexpr int m_timeout = 15;
	static constexpr int m_max_peers = 50;

	std::vector<std::vector<std::string>> m_tracker_addresses;
	size_t m_last_tracker_tier;
	size_t m_last_tracker_tier_index;
	TrackerConnection m_tracker;

	std::array<PeerConnection, m_max_peers> m_peers;

	// the last one pollfd is tracker's pollfd
	std::array<struct pollfd, m_max_peers + 1> m_fds = []() {
		std::array<struct pollfd, m_max_peers + 1> ret;
		ret.fill({ -1, 0, 0 });
		return ret;
	}();

	std::string m_connection_id = utils::generate_random_connection_id();

public:
	PeerPool() = default;
	void set_tracker_addresses(const std::vector<std::vector<std::string>> &tracker_addresses);
	void connect_to_tracker();
	int proceed();
};
