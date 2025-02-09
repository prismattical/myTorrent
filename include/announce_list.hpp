#pragma once

#include <string>
#include <vector>

/**
 * @brief Container for several different announce URLs
 * 
 * This is the class that represents announce-list extension.
 * Documentation for it is here
 * http://bittorrent.org/beps/bep_0012.html
 * 
 * In short, all URLs are divided in tiers. The order in which client
 * should connect to URLs is based on this tiers. This class implements 
 * the rules stated in documentation.
 */
class AnnounceList {
	std::vector<std::vector<std::string>> m_announce_list;
	size_t m_i = 0;
	size_t m_j = 0;

public:
	AnnounceList() = default;
	/**
	 * @brief Creates announce-list from an existing list
	 * 
	 * This method should be called with data read from metainfo file
	 */
	explicit AnnounceList(std::vector<std::vector<std::string>> &&announce_list);

	/**
	 * @brief Set index to first URL in first tier
	 */
	void reset_index();
	/**
	* @brief Moves index to next tracker
	* 
	* @return 0 if move was successful
	*		  1 if index stayed the same (end of list reached)
	*/
	[[nodiscard]] int move_index_next();
	/**
	 * @brief Moves index to previous tracker
	 * 
	 * @return 0 if move was successful
	 * 		   1 if index stayed the same (beginning of list reached)
	 */
	[[nodiscard]] int move_index_prev();
	/**
	 * @brief Get the pair of current tracker ip and port
	 * 
	 * @return first value of pair is ip, second is port
	 */
	[[nodiscard]] std::pair<std::string, std::string> get_current_tracker() const;
	/**
	 * @brief Moves a current tracker to the highest place in the tier
	 *
	 * According to documentation, this should be done whenever you 
	 * successfully connect to the tracker.
	 * This method **do not** reset index. You probably should call
	 * reset_index() method after this
	 */
	void move_current_tracker_to_top();
};
