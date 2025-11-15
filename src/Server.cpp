/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: webserv                                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/15                              #+#    #+#             */
/*   Updated: 2025/11/15                             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <iostream>
#include <sstream>
#include <cstdlib>

Server::Server(const Config& config)
	: _config(config) {
}

Server::~Server() {
	cleanup();
}

void Server::run() {
	std::cout << "\n=== Starting Server ===" << std::endl;
	
	initListeners();
	
	std::cout << "\n✅ All listeners initialized successfully!" << std::endl;
	std::cout << "Server is ready to accept connections." << std::endl;
	std::cout << "\nPress Ctrl+C to stop the server..." << std::endl;
	
	// TODO: Step 3 - イベントループの実装
	// 今は何もせずに終了
}

void Server::initListeners() {
	for (size_t i = 0; i < _config.servers.size(); ++i) {
		ServerConfig& serverConfig = _config.servers[i];
		
		for (size_t j = 0; j < serverConfig.listen.size(); ++j) {
			std::string listenStr = serverConfig.listen[j];
			std::string host;
			int port;
			
			// "host:port" または ":port" または "port" の形式をパース
			size_t colonPos = listenStr.find(':');
			if (colonPos != std::string::npos) {
				// "host:port" または ":port"
				host = listenStr.substr(0, colonPos);
				if (host.empty()) {
					host = "0.0.0.0"; // デフォルトは全インターフェース
				}
				port = std::atoi(listenStr.substr(colonPos + 1).c_str());
			} else {
				// "port" のみ
				host = "0.0.0.0";
				port = std::atoi(listenStr.c_str());
			}
			
			// ポート番号の検証
			if (port <= 0 || port > 65535) {
				std::ostringstream oss;
				oss << "Invalid port number: " << port;
				throw std::runtime_error(oss.str());
			}
			
			// Listenerを作成して初期化
			Listener* listener = new Listener(host, port, &serverConfig);
			try {
				listener->init();
				_listeners.push_back(listener);
			} catch (const std::exception& e) {
				delete listener;
				throw;
			}
		}
	}
	
	if (_listeners.empty()) {
		throw std::runtime_error("No listeners configured");
	}
}

void Server::cleanup() {
	for (size_t i = 0; i < _listeners.size(); ++i) {
		delete _listeners[i];
	}
	_listeners.clear();
}
