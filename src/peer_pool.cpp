#include "peer_pool.hpp"

#include "tracker_connection.hpp"

#include <poll.h>

#include <string>

PeerPool::PeerPool(std::string connection_id)
	: m_connection_id(std::move(connection_id))
{
}


