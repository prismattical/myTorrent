#include "tracker_connection.hpp"

#include "socket.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

TrackerConnection::TrackerConnection(const std::string &hostname, const std::string &port,
				     std::span<const uint8_t> connection_id,
				     const std::string &info_hash)
	: m_socket(hostname, port, Socket::IP_UNSPEC)
{
	const std::string connection_id_str =
		std::string(connection_id.begin(), connection_id.end());

	const std::string query = "/announce?info_hash=" + info_hash + "&" +
				  "peer_id=" + connection_id_str + "&" + "port=" + "8765";

	std::string request_str;
	request_str += "GET " + query + " " + "HTTP/1.1" + "\r\n";
	request_str += "Host: " + hostname + "\r\n";
	request_str += "Connection: Close\r\n";
	request_str += "Accept: text/plain\r\n";
	request_str += "\r\n";

	m_send_buffer = std::vector<uint8_t>(request_str.begin(), request_str.end());
}

int TrackerConnection::get_socket_fd() const
{
	return m_socket.get_fd();
}

bool TrackerConnection::should_wait_for_send() const
{
	return !m_request_sent;
}

std::span<const uint8_t> TrackerConnection::view_recv_message() const
{
	return { m_recv_buffer.begin(), m_recv_offset };
}

std::vector<uint8_t> &&TrackerConnection::get_recv_message()
{
	return std::move(m_recv_buffer);
}

int TrackerConnection::send()
{
	m_socket.send(m_send_buffer, m_send_offset);
	if (m_send_offset == 0)
	{
		m_request_sent = true;
		return 0;
	}
	return 1;
}

int TrackerConnection::recv()
{
	try
	{
		m_socket.recv(m_recv_buffer, m_recv_offset);
	} catch (const Socket::ConnectionResetException &cre)
	{
		m_socket.~Socket();
		return 0;
	}
	return 1;
}

void TrackerConnection::set_timeout(long long seconds)
{
	m_tp = std::chrono::steady_clock::now();
	m_timeout = seconds;
	// std::cout << "Timeout was set to " << m_timeout << " seconds\n";
}

bool TrackerConnection::update_time()
{
	using namespace std::chrono;
	auto seconds_passed = duration_cast<seconds>(steady_clock::now() - m_tp).count();
	// std::cout << "Seconds passed since last tracker call: " << seconds_passed << "\n";
	return m_timeout != 0 && seconds_passed >= m_timeout;
}
