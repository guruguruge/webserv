/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Poller.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: webserv                                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/15                              #+#    #+#             */
/*   Updated: 2025/11/15                             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef POLLER_HPP
#define POLLER_HPP

#include <poll.h>
#include <vector>
#include <map>
#include <cstddef>

class Poller {
public:
	Poller();
	~Poller();

	void add(int fd, short events);
	void modify(int fd, short events);
	void remove(int fd);

	int wait(int timeoutMs);
	const std::vector<struct pollfd>& getEvents() const;

private:
	std::vector<struct pollfd> _fds;
	std::map<int, size_t> _fdToIndex; // fd -> _fds のインデックス

	void rebuildIndexMap();
};

#endif
