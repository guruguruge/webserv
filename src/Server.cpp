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
#include "Router.hpp"
#include "StaticFileHandler.hpp"
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <csignal>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cerrno>

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
	
	for (size_t i = 0; i < _listeners.size(); ++i) {
		if (_listeners[i]->getFd() == pfd.fd) {
			isListenFd = true;
			break;
		}
	}
	
	if (isListenFd && (pfd.revents & POLLIN)) {
		// listen ソケットに POLLIN イベント - 新しい接続を受け入れる
		acceptNewClient(pfd.fd);
	} else if (_clients.find(pfd.fd) != _clients.end()) {
		// クライアントソケットのイベント
		if (pfd.revents & POLLIN) {
			handleClientRead(pfd.fd);
		} else if (pfd.revents & POLLOUT) {
			handleClientWrite(pfd.fd);
		} else if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
			std::cerr << "⚠️  Error on client fd " << pfd.fd << std::endl;
			closeClient(pfd.fd);
		}
	} else {
		// その他のエラー
		if (pfd.revents & POLLERR) {
			std::cerr << "⚠️  POLLERR on fd: " << pfd.fd << std::endl;
		} else if (pfd.revents & POLLHUP) {
			std::cerr << "⚠️  POLLHUP on fd: " << pfd.fd << std::endl;
		} else if (pfd.revents & POLLNVAL) {
			std::cerr << "⚠️  POLLNVAL on fd: " << pfd.fd << std::endl;
		}
	}
}

void Server::acceptNewClient(int listenFd) {
	// listenFdに対応するListenerを見つける
	Listener* listener = NULL;
	for (size_t i = 0; i < _listeners.size(); ++i) {
		if (_listeners[i]->getFd() == listenFd) {
			listener = _listeners[i];
			break;
		}
	}
	
	if (!listener) {
		std::cerr << "⚠️  Listener not found for fd: " << listenFd << std::endl;
		return;
	}
	
	// accept()で新しい接続を受け入れる
	struct sockaddr_in clientAddr;
	socklen_t clientAddrLen = sizeof(clientAddr);
	int clientFd = accept(listenFd, (struct sockaddr*)&clientAddr, &clientAddrLen);
	
	if (clientFd < 0) {
		std::cerr << "⚠️  accept() failed" << std::endl;
		return;
	}
	
	// クライアントソケットを非ブロッキングに設定
	int flags = fcntl(clientFd, F_GETFL, 0);
	if (flags == -1) {
		std::cerr << "⚠️  fcntl(F_GETFL) failed" << std::endl;
		close(clientFd);
		return;
	}
	
	if (fcntl(clientFd, F_SETFL, flags | O_NONBLOCK) == -1) {
		std::cerr << "⚠️  fcntl(F_SETFL) failed" << std::endl;
		close(clientFd);
		return;
	}
	
	// ClientConnectionを作成
	ClientConnection* client = new ClientConnection(clientFd, listener->getServerConfig());
	_clients[clientFd] = client;
	
	// Pollerに追加（POLLIN で読み取り待機）
	_poller.add(clientFd, POLLIN);
	
	std::cout << "✅ New client connected (fd: " << clientFd << ") on " 
	          << listener->getHost() << ":" << listener->getPort() << std::endl;
	
	(void)client; // 未使用警告の回避（将来的に使用する）
}

void Server::handleClientRead(int clientFd) {
	ClientConnection* client = _clients[clientFd];
	
	ssize_t n = client->readFromSocket();
	
	if (n > 0) {
		std::cout << "📖 Read " << n << " bytes from client (fd: " << clientFd << ")" << std::endl;
		
		// HTTPリクエストをパース
		std::string& recvBuf = client->getRecvBuffer();
		HttpRequestParser& parser = client->getParser();
		
		parser.parse(recvBuf.c_str(), recvBuf.size());
		
		if (parser.getState() == HttpRequestParser::PARSE_DONE) {
			// パース完了
			const HttpRequest& req = parser.getRequest();
			
			std::cout << "✅ HTTP Request parsed successfully:" << std::endl;
			std::cout << "   Method: " << req.method << std::endl;
			std::cout << "   Path: " << req.path << std::endl;
			std::cout << "   Query: " << req.query << std::endl;
			std::cout << "   HTTP Version: " << req.httpVersion << std::endl;
			std::cout << "   Host: " << req.host << std::endl;
			
			// ルーティング: サーバーとロケーションを選択
			const ServerConfig* serverConfig = Router::findServer(_config, req);
			const LocationConfig* locationConfig = NULL;
			
			if (serverConfig) {
				locationConfig = Router::findLocation(*serverConfig, req.path);
				std::cout << "   Server: " << (serverConfig->serverName.empty() ? "(default)" : serverConfig->serverName) << std::endl;
				std::cout << "   Location: " << (locationConfig ? locationConfig->path : "(none)") << std::endl;
			}
			
			// レスポンス生成
			HttpResponse response;
			
			if (!serverConfig) {
				// サーバーが見つからない（通常は発生しない）
				response.setStatusCode(500);
				response.setBody("Internal Server Error");
			} else if (req.method == "GET" || req.method == "HEAD") {
				// 静的ファイルハンドラーで処理
				response = StaticFileHandler::handleGet(req, *serverConfig, locationConfig);
			} else {
				// その他のメソッドは未実装
				response.setStatusCode(501);
				response.setBody("Not Implemented");
			}
			
			// レスポンスをシリアライズして送信バッファに設定
			client->getSendBuffer() = response.serialize();
			
			std::cout << "   Response: " << response.getStatusCode() << " " 
			          << response.getReasonPhrase() << std::endl;
			
			// 受信バッファをクリア
			recvBuf.clear();
			
			// 書き込み可能になるまで待機
			_poller.modify(clientFd, POLLOUT);
			client->setState(ClientConnection::WRITING);
		} else if (parser.getState() == HttpRequestParser::PARSE_ERROR) {
			// パースエラー
			std::cerr << "❌ HTTP parse error: " << parser.getErrorMessage() << std::endl;
			
			// 400 Bad Request を返す
			std::string response = "HTTP/1.1 400 Bad Request\r\n"
			                       "Content-Type: text/plain\r\n"
			                       "Content-Length: 11\r\n"
			                       "\r\n"
			                       "Bad Request";
			
			client->getSendBuffer() = response;
			recvBuf.clear();
			
			_poller.modify(clientFd, POLLOUT);
			client->setState(ClientConnection::WRITING);
		} else {
			// パース継続中 - 次のデータを待つ
			std::cout << "   Waiting for more data (current state: " << parser.getState() << ")" << std::endl;
		}
	} else if (n == 0) {
		// クライアントが接続を閉じた
		std::cout << "🔌 Client disconnected (fd: " << clientFd << ")" << std::endl;
		closeClient(clientFd);
	} else {
		// エラー（EAGAINやEWOULDBLOCKは正常）
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			std::cerr << "⚠️  Read error on client (fd: " << clientFd << ")" << std::endl;
			closeClient(clientFd);
		}
	}
}

void Server::handleClientWrite(int clientFd) {
	ClientConnection* client = _clients[clientFd];
	
	ssize_t n = client->writeToSocket();
	
	if (n > 0) {
		std::cout << "📝 Wrote " << n << " bytes to client (fd: " << clientFd << ")" << std::endl;
		
		// 全部送信完了したか確認
		if (client->getSendBuffer().empty()) {
			std::cout << "✅ Response sent completely to client (fd: " << clientFd << ")" << std::endl;
			
			// TODO: keep-alive のチェック
			// 今は接続を閉じる
			closeClient(clientFd);
		}
	} else if (n < 0) {
		// エラー（EAGAINやEWOULDBLOCKは正常）
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			std::cerr << "⚠️  Write error on client (fd: " << clientFd << ")" << std::endl;
			closeClient(clientFd);
		}
	}
}

void Server::closeClient(int clientFd) {
	std::map<int, ClientConnection*>::iterator it = _clients.find(clientFd);
	if (it == _clients.end()) {
		return;
	}
	
	_poller.remove(clientFd);
	delete it->second;
	_clients.erase(it);
	
	std::cout << "❌ Client connection closed (fd: " << clientFd << ")" << std::endl;
}

void Server::cleanup() {
	// すべてのクライアントを閉じる
	for (std::map<int, ClientConnection*>::iterator it = _clients.begin();
	     it != _clients.end(); ++it) {
		delete it->second;
	}
	_clients.clear();
	
	// すべてのリスナーを閉じる
	for (size_t i = 0; i < _listeners.size(); ++i) {
		delete _listeners[i];
	}
	_listeners.clear();
}
