#include "announce_list.hpp"

#include "utils.hpp"

#include <tuple>

// AnnounceList ------------------------------------------------------------------------

AnnounceList::AnnounceList(std::vector<std::vector<std::string>> &&announce_list)
	: m_announce_list(std::move(announce_list))
{
}

void AnnounceList::reset_index()
{
	m_i = 0;
	m_j = 0;
}

int AnnounceList::move_index_next()
{
	if (m_j + 1 < m_announce_list[m_i].size())
	{
		++m_j;
	} else if (m_i + 1 < m_announce_list.size())
	{
		++m_i;
		m_j = 0;
	} else
	{
		return 1;
	}
	return 0;
}

int AnnounceList::move_index_prev()
{
	if (m_j != 0)
	{
		--m_j;
	} else if (m_i != 0)
	{
		--m_i;
		m_j = m_announce_list[m_i].size() - 1;
	} else
	{
		return 1;
	}
	return 0;
}

std::pair<std::string, std::string> AnnounceList::get_current_tracker() const
{
	const auto announce_url = m_announce_list[m_i][m_j];

	const auto [protocol, domain_name, port] = utils::parse_announce_url(announce_url);

	// TODO: handle non-http protocols

	return { domain_name, port };
}

void AnnounceList::move_current_tracker_to_top()
{
	std::swap(m_announce_list[m_i][m_j], m_announce_list[m_i][0]);
}
