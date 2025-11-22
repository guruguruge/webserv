/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ClientConnection.hpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: webserv                                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/15                              #+#    #+#             */
/*   Updated: 2025/11/15                             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENT_CONNECTION_HPP
#define CLIENT_CONNECTION_HPP

#include "Config.hpp"
#include "HttpRequestParser.hpp"
#include <string>
#include <ctime>
#include <sys/types.h>

class ClientConnection {
public:
	enum State {
		READING,
		WRITING,
		CGI_WAIT,
		CLOSED
	};

	ClientConnection();
	ClientConnection(int fd, ServerConfig* serverConfig);
	~ClientConnection();

	int  getFd() const;
	State getState() const;
	void setState(State s);

	ServerConfig* getServerConfig() const;

	ssize_t readFromSocket();
	ssize_t writeToSocket();

	std::string& getRecvBuffer();
	std::string& getSendBuffer();

	// HTTPリクエストパーサー
	HttpRequestParser& getParser();
	bool isRequestComplete() const;

	// keep-alive 関連
	void updateActivity();
	time_t getLastActivity() const;
	void setKeepAlive(bool keep);
	bool isKeepAlive() const;

	// CGI 関連
	void setCgiStdoutFd(int fd);
	int  getCgiStdoutFd() const;
	void appendCgiOutput(const std::string& data);
	std::string& getCgiOutputBuffer();

private:
	int           _fd;
	State         _state;
	ServerConfig* _serverConfig;

	std::string   _recvBuffer;
	std::string   _sendBuffer;

	HttpRequestParser _parser;

	bool          _keepAlive;
	time_t        _lastActivity;

	int           _cgiStdoutFd;
	std::string   _cgiOutputBuffer;
};

#endif
