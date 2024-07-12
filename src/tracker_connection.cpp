#include "tracker_connection.hpp"

#include "socket.hpp"

#include <string>
#include <utility>
#include <vector>

TrackerConnection::TrackerConnection(std::string hostname, std::string port,
				     std::string connection_id)
	: m_hostname{ std::move(hostname) }
	, m_port{ std::move(port) }
	, m_connection_id{ std::move(connection_id) }
{
}

void TrackerConnection::send_http_request(const std::string &trk_hostname,
					  const std::string &trk_resource,
					  const std::string &info_hash,
					  const std::string &upload_port) const
{
	m_socket = Socket(m_hostname, m_port, Socket::TCP, Socket::IP_UNSPEC);

	const std::string query = trk_resource + "?" + "info_hash=" + info_hash + "&" +
				  "peer_id=" + m_connection_id + "&" + "port=" + upload_port;

	std::string request_str;
	request_str += "GET " + query + " " + "HTTP/1.1" + "\r\n";
	request_str += "Host: " + trk_hostname + "\r\n";
	request_str += "Connection: Close\r\n";
	request_str += "Accept: text/plain\r\n";
	request_str += "\r\n";

	const std::vector<unsigned char> request_buff(request_str.begin(), request_str.end());

	m_socket.send_all(request_buff);
}

std::string TrackerConnection::recv_http_request()
{
	std::vector<unsigned char> vec{ m_socket.recv_until_close() };
	return std::string(vec.begin(), vec.end());
}
