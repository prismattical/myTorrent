#include "peer_pool.hpp"

#include "tracker_connection.hpp"

#include <poll.h>

#include <cstring>
#include <stdexcept>
#include <string>

void PeerPool::set_tracker_addresses(const std::vector<std::vector<std::string>> &tracker_addresses)
{
	m_tracker_addresses = tracker_addresses;
}

void PeerPool::connect_to_tracker()
{
	const auto tracker_address =
		m_tracker_addresses[m_last_tracker_tier][m_last_tracker_tier_index];

	// TODO: rewrite parse_url function
	const auto [protocol, endpoint, _] = utils::parse_url(tracker_address);
	const auto [domain_name, port] = utils::parse_endpoint(endpoint);

	m_tracker = TrackerConnection(domain_name, port, m_connection_id);
	m_fds.back().fd = m_tracker.get_socket_fd();
	m_fds.back().events = POLLOUT;
}

int PeerPool::proceed()
{
	int rc = poll(m_fds.data(), m_fds.size(), m_timeout);
	if (rc > 0)
	{
		// on success
		if (m_fds.back().fd != -1)
		{
			if ((m_fds.back().revents & POLLOUT) != 0)
			{
				if (m_tracker.send_http("/announce", "", "8765") == 1)
				{
					m_fds.back().events = POLLOUT;
				} else
				{
					m_fds.back().events = POLLIN;
				}
			} else if ((m_fds.back().revents & POLLIN) != 0)
			{
				std::string peers_bencoded = m_tracker.recv_http().value_or("-1");
				if (peers_bencoded == "-1")
				{
					m_fds.back().events = POLLIN;
				} else
				{
					// TODO: proceed peers responce
					m_fds.back().fd = -1;
				}
			}
		}
	} else if (rc == 0)
	{
		// on timeout
		throw std::runtime_error("poll() timeout expired");
	} else
	{
		// on error
		throw std::runtime_error(std::string("poll(): ") + strerror(errno));
	}
}
