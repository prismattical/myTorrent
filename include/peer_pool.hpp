#pragma once

#include "peer_connection.hpp"
#include "tracker_connection.hpp"

#include <poll.h>

#include <array>
#include <deque>
#include <string>
#include <tuple>
#include <vector>

class PeerPool {
private:
	static constexpr int m_first_tracker_timeout = 120;
	static constexpr int m_max_peers = 50;

	std::vector<TrackerConnection> m_trackers;

	std::deque<std::tuple<std::string, std::string>> m_possible_peers;
	std::array<PeerConnection, m_max_peers> m_peers;

	std::vector<struct pollfd> m_fds;

	std::string m_connection_id;

public:
	PeerPool() = default;
	PeerPool(std::string connection_id);
	void connect_to_tracker(const std::string &domain_name, const std::string &port);
};
