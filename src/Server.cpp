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
#include <csignal>

// シグナルハンドリング用のグローバル変数
static volatile sig_atomic_t g_running = 1;

static void signalHandler(int signum) {
	(void)signum;
	g_running = 0;
}

Server::Server(const Config& config)
	: _config(config), _running(true) {
}

Server::~Server() {
	cleanup();
}

void Server::run() {
	std::cout << "\n=== Starting Server ===" << std::endl;
	
	// シグナルハンドラの設定
	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);
	
	initListeners();
	
	std::cout << "\n✅ All listeners initialized successfully!" << std::endl;
	std::cout << "Server is ready to accept connections." << std::endl;
	std::cout << "\nPress Ctrl+C to stop the server..." << std::endl;
	
	// イベントループの開始
	eventLoop();
	
	std::cout << "\n\n🛑 Server stopped gracefully." << std::endl;
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
				
				// Pollerにlistenソケットを追加（POLLIN イベント）
				_poller.add(listener->getFd(), POLLIN);
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

void Server::eventLoop() {
	std::cout << "\n=== Event Loop Started ===" << std::endl;
	
	int eventCount = 0;
	
	while (_running && g_running) {
		// poll() で待機（タイムアウト: 1000ms = 1秒）
		int ret = _poller.wait(1000);
		
		if (ret < 0) {
			std::cerr << "poll() error" << std::endl;
			break;
		}
		
		if (ret == 0) {
			// タイムアウト（イベントなし）
			continue;
		}
		
		// イベントを処理
		const std::vector<struct pollfd>& events = _poller.getEvents();
		for (size_t i = 0; i < events.size(); ++i) {
			if (events[i].revents != 0) {
				handlePollEvent(events[i]);
				eventCount++;
			}
		}
	}
	
	std::cout << "\nTotal events processed: " << eventCount << std::endl;
}

void Server::handlePollEvent(const struct pollfd& pfd) {
	// listen fdかどうかをチェック
	bool isListenFd = false;
	Listener* listener = NULL;
	
	for (size_t i = 0; i < _listeners.size(); ++i) {
		if (_listeners[i]->getFd() == pfd.fd) {
			isListenFd = true;
			listener = _listeners[i];
			break;
		}
	}
	
	if (isListenFd && (pfd.revents & POLLIN)) {
		// listen ソケットに POLLIN イベント
		std::cout << "📥 POLLIN event on listen socket (fd: " << pfd.fd << ")" << std::endl;
		std::cout << "   Ready to accept new connection on " 
		          << listener->getHost() << ":" << listener->getPort() << std::endl;
		
		// TODO: Step 4 - accept() して ClientConnection を作成
	} else if (pfd.revents & POLLERR) {
		std::cerr << "⚠️  POLLERR on fd: " << pfd.fd << std::endl;
	} else if (pfd.revents & POLLHUP) {
		std::cerr << "⚠️  POLLHUP on fd: " << pfd.fd << std::endl;
	} else if (pfd.revents & POLLNVAL) {
		std::cerr << "⚠️  POLLNVAL on fd: " << pfd.fd << std::endl;
	}
}

void Server::cleanup() {
	for (size_t i = 0; i < _listeners.size(); ++i) {
		delete _listeners[i];
	}
	_listeners.clear();
}
