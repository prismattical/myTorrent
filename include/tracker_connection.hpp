#pragma once

#include "socket.hpp"

#include <chrono>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

/**
 * @brief Struct that contains input data to generate an HTTP request
 * @note Many things are not implemented. Implementing them requires 
 * creating some sort of fastresume file that stores stats about total amount
 * of bytes uploaded and other data
 */
struct TrackerRequestParams {
	std::string info_hash; // must be present
	std::span<const std::uint8_t> peer_id; // must be present
	std::string port = "8765"; // must be present, but unused
	std::string uploaded; // total amount of bytes uploaded, unused
	std::string downloaded; // total amount of bytes downloaded, unused
	std::string left; // number of bytes that are missing, unused
	bool compact = false; // false doesn't guarantee, that tracker won't send compact, unused
	bool no_peer_id = false;
	std::string event; // if empty, then no event is sent, unused
	std::string ip;
	std::string numwant;
	std::string key;
	std::string trackerid;
};

class TrackerConnection {
	static constexpr int recv_buffer_size = 4096;

	TCPClient m_socket;

	std::vector<std::uint8_t> m_send_buffer;
	size_t m_send_offset = 0;

	std::vector<std::uint8_t> m_recv_buffer = std::vector<std::uint8_t>(recv_buffer_size);
	size_t m_recv_offset = 0;

	bool m_request_sent = false;

	std::chrono::steady_clock::time_point m_tp;
	long long m_timeout = 0;
	/**
	 * @brief Generates query string
	 * 
	 * @param param The struct that contains data needed to generate query
	 * @return The query string
	 */
	static std::string generate_query(const TrackerRequestParams &param);

public:
	TrackerConnection() = default;
	/**
	 * @brief Starts a connection with the HTTP tracker and generates request to send
	 * 
	 * @param hostname The domain name of the server
	 * @param port The port on the server
	 * @param param The struct that contains data needed to generate request
	 * @throws std::runtime_error If failed to connect
	 */
	TrackerConnection(const std::string &hostname, const std::string &port,
			  const TrackerRequestParams &param);
	/**
	 * @brief Starts a connection with the HTTP tracker and generates request to send
	 * 
	 * @param hostname The domain name of the server
	 * @param port The port on the server
	 * @param param The struct that contains data needed to generate request
	 * @throws std::runtime_error If failed to connect
	 */
	void connect(const std::string &hostname, const std::string &port,
		     const TrackerRequestParams &param);
	/**
	 * @brief Terminates the connection if it was open
	 */
	void disconnect();
	/**
	 * @brief Returns the underlying file descriptor
	 * 
	 * @return The file descriptor integer or -1 if socket is not open
	 * @note The caller should not close the descriptor manually
	 */
	[[nodiscard]] int get_socket_fd() const;
	/**
	 * @brief Checks whether async manager should wait for send()
	 * 
	 * @return true if there is data to be sent
	 * @return false if there is no data to be sent
	 */
	[[nodiscard]] bool should_wait_for_send() const;
	/**
	 * @brief Returns the const span of received data
	 * 
	 * @return The const span of the received data
	 */
	[[nodiscard]] std::span<const std::uint8_t> view_recv_message() const;
	/**
	 * @brief Moves the received data to the caller
	 * 
	 * @return The rvalue reference to the vector of the received data
	 */
	[[nodiscard]] std::vector<std::uint8_t> &&get_recv_message();
	/**
	 * @brief Sends the HTTP request to the server
	 * 
	 * @return 0 on successful send
	 * @return 1 on partial send
	 * @throw std::runtime_error on socket failure
	 */
	[[nodiscard]] int send();
	/**
	 * @brief Receives the HTTP response from the server
	 * 
	 * @return 0 on successful recv
	 * @return 1 on partial recv
	 * @throw std::runtime_error on socket failure or if response is too big
	 */
	[[nodiscard]] int recv();
	/**
	 * @brief Sets the timeout. After the timeout update_time() will return true
	 * and reset the timer.
	 * 
	 * @param seconds The amount of seconds to wait until update_time will return true
	 */
	void set_timeout(long long seconds);
	/**
	 * @brief Checks whether timeout expired (if such was set)
	 * @note This function returns true only once, after which it starts to act
	 * as if timer was not set
	 * @return true if timeout has expired
	 * @return false if timeout was not set or has not yet expired
	 */
	[[nodiscard]] bool update_time();
};
