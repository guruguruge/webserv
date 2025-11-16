/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ClientConnection.cpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: webserv                                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/15                              #+#    #+#             */
/*   Updated: 2025/11/15                             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ClientConnection.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <cstring>
#include <iostream>

ClientConnection::ClientConnection()
	: _fd(-1), _state(READING), _serverConfig(NULL),
	  _keepAlive(true), _lastActivity(0), _cgiStdoutFd(-1) {
	updateActivity();
}

ClientConnection::ClientConnection(int fd, ServerConfig* serverConfig)
	: _fd(fd), _state(READING), _serverConfig(serverConfig),
	  _keepAlive(true), _lastActivity(0), _cgiStdoutFd(-1) {
	updateActivity();
}

ClientConnection::~ClientConnection() {
	if (_fd != -1) {
		close(_fd);
	}
	if (_cgiStdoutFd != -1) {
		close(_cgiStdoutFd);
	}
}

int ClientConnection::getFd() const {
	return _fd;
}

ClientConnection::State ClientConnection::getState() const {
	return _state;
}

void ClientConnection::setState(State s) {
	_state = s;
}

ServerConfig* ClientConnection::getServerConfig() const {
	return _serverConfig;
}

ssize_t ClientConnection::readFromSocket() {
	char buffer[4096];
	ssize_t n = recv(_fd, buffer, sizeof(buffer), 0);
	
	if (n > 0) {
		_recvBuffer.append(buffer, n);
		updateActivity();
	}
	
	return n;
}

ssize_t ClientConnection::writeToSocket() {
	if (_sendBuffer.empty()) {
		return 0;
	}
	
	ssize_t n = send(_fd, _sendBuffer.c_str(), _sendBuffer.size(), 0);
	
	if (n > 0) {
		_sendBuffer.erase(0, n);
		updateActivity();
	}
	
	return n;
}

std::string& ClientConnection::getRecvBuffer() {
	return _recvBuffer;
}

std::string& ClientConnection::getSendBuffer() {
	return _sendBuffer;
}

HttpRequestParser& ClientConnection::getParser() {
	return _parser;
}

bool ClientConnection::isRequestComplete() const {
	return _parser.getState() == HttpRequestParser::PARSE_DONE;
}

void ClientConnection::updateActivity() {
	_lastActivity = std::time(NULL);
}

time_t ClientConnection::getLastActivity() const {
	return _lastActivity;
}

void ClientConnection::setKeepAlive(bool keep) {
	_keepAlive = keep;
}

bool ClientConnection::isKeepAlive() const {
	return _keepAlive;
}

void ClientConnection::setCgiStdoutFd(int fd) {
	_cgiStdoutFd = fd;
}

int ClientConnection::getCgiStdoutFd() const {
	return _cgiStdoutFd;
}

void ClientConnection::appendCgiOutput(const std::string& data) {
	_cgiOutputBuffer.append(data);
}

std::string& ClientConnection::getCgiOutputBuffer() {
	return _cgiOutputBuffer;
}
