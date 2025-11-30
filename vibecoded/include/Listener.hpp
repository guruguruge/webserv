/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Listener.hpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: webserv                                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/15                              #+#    #+#             */
/*   Updated: 2025/11/15                             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef LISTENER_HPP
#define LISTENER_HPP

#include "Config.hpp"
#include <string>

class Listener {
public:
	Listener(const std::string& host, int port, ServerConfig* config);
	~Listener();

	void init(); // socket/bind/listen/non-block
	int  getFd() const;
	ServerConfig* getServerConfig() const;
	std::string getHost() const;
	int getPort() const;

private:
	int         _fd;
	std::string _host;
	int         _port;
	ServerConfig* _config;

	void createSocket();
	void setNonBlocking();
	void setReuseAddr();
	void bindSocket();
	void listenSocket();
};

#endif
