#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <tuple>

/**
 * @brief RAII wrapper for non-blocking TCP client socket
 */
class TCPClient {
	int m_socket = -1;

public:
	/**
	 * @brief Construct a new TCPClient object without opening a socket
	 */
	TCPClient() = default;
	/**
	 * @brief Opens a new socket and connects it to the endpoint
	 * 
	 * @param hostname The hostname (IPv4 or IPv6 address, or domain name) of the server
	 * @param port The port on the server
	 * @throws std::runtime_error If opening or connection failed
	 */
	TCPClient(const std::string &hostname, const std::string &port);

	TCPClient(const TCPClient &other) = delete;
	TCPClient &operator=(const TCPClient &other) = delete;

	TCPClient(TCPClient &&other) noexcept;
	TCPClient &operator=(TCPClient &&other) noexcept;
	/**
	 * @brief Opens a new socket and connects it to the endpoint
	 * 
	 * @param hostname The hostname (IPv4 or IPv6 address, or domain name) of the server
	 * @param port The port on the server
	 * @throws std::runtime_error If opening or connection failed
	 */
	void connect(const std::string &hostname, const std::string &port);
	/**
	 * @brief Checks whether the socket successfully connected to the endpoint
	 * 
	 * This function may be called after connect() returned -1 with EINPROGRESS errno set
	 * and send() became available to validate connect. Alternatively, it is possible
	 * to just try calling send() and check for errors.
	 * @return true if connect() was successful
	 * @return false if connect() failed
	 */
	[[nodiscard]] bool connect_successful() const;
	/**
	 * @brief Sends data to the peer
	 * 
	 * This function may send the data partially. In which case the caller should call the function
	 * until the sum of all bytes sent is equal to the size of the initial span.
	 * @param buffer The span that contains data to send
	 * @return The non-negative value indicating the number of bytes successfully sent
	 * @return -1 indicating that the call would normally block and no data was sent
	 * @throws std::runtime_error If send() returned an error
	 */
	[[nodiscard]] long send(std::span<const uint8_t> buffer) const;
	/**
	 * @brief Receives data from the peer
	 * 
	 * This function may recv the data partially. In which case the caller should call the function
	 * until the sum of all bytes recved is equal to the size of the initial span or 0 is returned.
	 * @param buffer The span where the data will be written to
	 * @return The positive value indicating taht the number of bytes successfully recved
	 * @return -1 indicating that the call would normally block and no data was recved
	 * @return 0 if connection was terminated by the peer
	 * @throws std::runtime_error If recv() returned an error
	 * @note Passing the span of size 0 may result in returning 0 too
	 */
	[[nodiscard]] long recv(std::span<uint8_t> buffer) const;
	/**
	 * @brief Receives data from the peer
	 * 
	 * This function may recv the data partially. In which case the caller should call the function
	 * until the sum of all bytes recved is equal to the size of the initial span or 0 is returned.
	 *
 	 * @note This function is similar to other recv() method, but reports a termination of connection
	 * 		 as an exception instead of returning 0
	 *
	 * @param buffer The span where the data will be written to
	 * @return The positive value indicating taht the number of bytes successfully recved
	 * @return -1 indicating that the call would normally block and no data was recved
	 * @throws std::runtime_error If recv() returned an error or connection was terminated by the peer
	 * @note Passing the span of size 0 may result in std::runtime_error thrown, so don't do it
	 */
	[[nodiscard]] long recv2(std::span<uint8_t> buffer) const;

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
	[[nodiscard]] int get_fd() const;

	[[nodiscard]] bool connected() const;

	[[nodiscard]] std::tuple<std::string, std::string> get_peer_ip_and_port() const;

	static std::string ntop(uint32_t ip);

	~TCPClient();
};
