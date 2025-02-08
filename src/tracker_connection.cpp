#include "tracker_connection.hpp"

#include "socket.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

TrackerConnection::TrackerConnection(const std::string &hostname, const std::string &port,
				     const TrackerRequestParams &param)
{
	connect(hostname, port, param);
}
/**
 * @brief Generates query string
 * 
 * @param param The struct that contains data needed to generate query
 * @return The query string
 */
std::string generate_query(const TrackerRequestParams &param)
{
	const std::string peer_id = std::string(param.peer_id.begin(), param.peer_id.end());

	std::string query = "/announce?info_hash=" + param.info_hash + "&peer_id=" + peer_id +
			    "&port=" + param.port;
	if (param.compact)
	{
		query += "&compact=1";
	}
	if (param.no_peer_id && !param.compact)
	{
		query += "&no_peer_id";
	}
	if (!param.ip.empty())
	{
		query += "&ip=" + param.ip;
	}
	if (!param.numwant.empty())
	{
		query += "&numwant=" + param.numwant;
	}
	if (!param.key.empty())
	{
		query += "&key=" + param.key;
	}
	if (!param.trackerid.empty())
	{
		query += "&trackerid" + param.trackerid;
	}

	return query;
}

void TrackerConnection::connect(const std::string &hostname, const std::string &port,
				const TrackerRequestParams &param)

{
	if (m_socket.connected())
	{
		m_socket.disconnect();
	}

	m_socket.connect(hostname, port);

	m_send_offset = 0;
	m_recv_offset = 0;
	m_request_sent = false;
	m_timeout = 0;

	const std::string query = generate_query(param);

	std::string request_str;
	request_str += "GET " + query + " " + "HTTP/1.1" + "\r\n";
	request_str += "Host: " + hostname + "\r\n";
	request_str += "Connection: Close\r\n";
	request_str += "Accept: text/plain\r\n";
	request_str += "\r\n";

	m_send_buffer = std::vector<std::uint8_t>(request_str.begin(), request_str.end());
}

void TrackerConnection::disconnect()
{
	m_socket.disconnect();
}

int TrackerConnection::get_socket_fd() const
{
	return m_socket.get_fd();
}

bool TrackerConnection::should_wait_for_send() const
{
	return !m_request_sent;
}

std::span<const std::uint8_t> TrackerConnection::view_recv_message() const
{
	return { m_recv_buffer.begin(), m_recv_offset };
}

std::vector<std::uint8_t> &&TrackerConnection::get_recv_message()
{
	auto &&ret = std::move(m_recv_buffer);
	m_recv_buffer = std::vector<std::uint8_t>(recv_buffer_size);
	return std::move(ret);
}

int TrackerConnection::send()
{
	long ret = m_socket.send(
		{ m_send_buffer.data() + m_send_offset, m_send_buffer.size() - m_send_offset });

	// if something unexpected happened somewhen between poll() and send()
	if (ret == -1)
	{
		return 1;
	}

	m_send_offset += ret;

	// on full send
	if (m_send_offset == m_send_buffer.size())
	{
		m_request_sent = true;
		return 0;
	}

	// on partial send
	return 1;
}

int TrackerConnection::recv()
{
	long ret = m_socket.recv(
		{ m_recv_buffer.data() + m_recv_offset, m_recv_buffer.size() - m_recv_offset });

	// if expected data was dropped somewhen between poll() and recv()
	if (ret == -1)
	{
		return 1;
	}

	m_recv_offset += ret;

	// if buffer was filled up and there is no more space available
	if (m_recv_offset == m_recv_buffer.size())
	{
		std::cerr << "HTTP response is too large" << '\n';
		throw std::runtime_error("recv() failed");
	}

	// on full recv
	if (m_recv_offset == 0)
	{
		m_socket.disconnect();
		return 0;
	}

	// on partial recv
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
	using std::chrono::steady_clock;
	using std::chrono::seconds;
	if (m_timeout == 0)
	{
		return false;
	}

	auto seconds_passed = duration_cast<seconds>(steady_clock::now() - m_tp).count();
	// std::cout << "Seconds passed since last tracker call: " << seconds_passed << "\n";
	if (seconds_passed >= m_timeout)
	{
		m_timeout = 0;
		return true;
	}
	return false;
}
