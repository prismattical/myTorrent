#include "tracker_connection.hpp"

#include "socket.hpp"

#include <cstdint>
#include <stdexcept>
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

bool TrackerConnection::get_socket_status() const
{
	return m_socket.connected();
}

int TrackerConnection::get_socket_fd() const
{
	return m_socket.get_fd();
}

void TrackerConnection::connect()
{
	m_socket = Socket(m_hostname, m_port, Socket::TCP, Socket::IP_UNSPEC);
}

int TrackerConnection::send_http(const std::string &trk_hostname, const std::string &trk_resource,
				 const std::string &info_hash, const std::string &upload_port)
{
	switch (m_send_state)
	{
	case SendState::FORM_REQUEST: {
		m_socket.validate_connect();

		const std::string query = trk_resource + "?" + "info_hash=" + info_hash + "&" +
					  "peer_id=" + m_connection_id + "&" +
					  "port=" + upload_port;

		std::string request_str;
		request_str += "GET " + query + " " + "HTTP/1.1" + "\r\n";
		request_str += "Host: " + trk_hostname + "\r\n";
		request_str += "Connection: Close\r\n";
		request_str += "Accept: text/plain\r\n";
		request_str += "\r\n";

		m_send_buffer = std::vector<uint8_t>(request_str.begin(), request_str.end());

		m_send_state = SendState::SEND_REQUEST;

		// ! notice no break or return here
	}
	case SendState::SEND_REQUEST: {
		m_socket.send(m_send_buffer, m_send_offset);
		if (m_send_offset == 0)
		{
			// handle full send

			m_send_state = SendState::FORM_REQUEST;

			return 0;
		}
		return 1;
	}
	default: {
		throw std::runtime_error("Send state value is MAX_SEND_STATES");
	}
	}
}

std::optional<std::string> TrackerConnection::recv_http()
{
	try
	{
		m_socket.recv(m_recv_buffer, m_recv_offset);
	} catch (const Socket::ConnectionResetException &cre)
	{
		return std::string(m_recv_buffer.begin(), m_recv_buffer.end());
	}
	return {};
}
