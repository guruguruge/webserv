/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: webserv                                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/15                              #+#    #+#             */
/*   Updated: 2025/11/15                             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include "Config.hpp"
#include "Listener.hpp"
#include "Poller.hpp"
#include "ClientConnection.hpp"
#include <vector>
#include <map>

class Server {
public:
	Server(const Config& config);
	~Server();

	void run();

private:
	void initListeners();
	void eventLoop();
	void handlePollEvent(const struct pollfd& pfd);
	
	void acceptNewClient(int listenFd);
	void handleClientRead(int clientFd);
	void handleClientWrite(int clientFd);
	void closeClient(int clientFd);
	
	void cleanup();

	Config _config;
	std::vector<Listener*> _listeners;
	Poller _poller;
	std::map<int, ClientConnection*> _clients;
	bool _running;
};

#endif
