#pragma once

#include "socket.hpp"

#include <string>

class TrackerConnection {
	mutable Socket m_socket;
	std::string m_hostname;
	std::string m_port;
	std::string m_connection_id;

public:
	TrackerConnection( std::string hostname,  std::string port, std::string connection_id);

	void send_http_request(const std::string &trk_hostname, const std::string &trk_resource,
			       const std::string &info_hash, const std::string &upload_port) const;

	std::string recv_http_request();
};
