/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: webserv                                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/15                              #+#    #+#             */
/*   Updated: 2025/11/15                             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ConfigParser.hpp"
#include <iostream>
#include <sstream>
#include <cstdlib>

ConfigParser::ConfigParser(const std::string& path)
	: _path(path), _currentTokenIndex(0) {
}

ConfigParser::~ConfigParser() {
}

Config ConfigParser::parse() {
	Config config;

	tokenize();

	while (hasMoreTokens()) {
		std::string token = peekToken();
		if (token == "server") {
			parseServerBlock(config);
		} else {
			throw std::runtime_error("Unexpected token: " + token);
		}
	}

	if (config.servers.empty()) {
		throw std::runtime_error("No server block found in config file");
	}

	return config;
}

void ConfigParser::tokenize() {
	std::ifstream file(_path.c_str());
	if (!file.is_open()) {
		throw std::runtime_error("Failed to open config file: " + _path);
	}

	std::string line;
	while (std::getline(file, line)) {
		// コメントを削除
		size_t commentPos = line.find('#');
		if (commentPos != std::string::npos) {
			line = line.substr(0, commentPos);
		}

		// セミコロンと波括弧の前後にスペースを追加
		std::string processed;
		for (size_t i = 0; i < line.length(); ++i) {
			char c = line[i];
			if (c == ';' || c == '{' || c == '}') {
				processed += ' ';
				processed += c;
				processed += ' ';
			} else {
				processed += c;
			}
		}

		// トークンに分割
		std::istringstream iss(processed);
		std::string token;
		while (iss >> token) {
			_tokens.push_back(token);
		}
	}

	file.close();
}

void ConfigParser::parseServerBlock(Config& config) {
	expectToken("server");
	expectToken("{");

	ServerConfig server;

	while (hasMoreTokens() && peekToken() != "}") {
		std::string directive = nextToken();

		if (directive == "listen") {
			parseListenDirective(server);
		} else if (directive == "server_name") {
			parseServerNameDirective(server);
		} else if (directive == "error_page") {
			parseErrorPageDirective(server);
		} else if (directive == "client_max_body_size") {
			parseClientMaxBodySizeDirective(server);
		} else if (directive == "location") {
			parseLocationBlock(server);
		} else {
			throw std::runtime_error("Unknown directive in server block: " + directive);
		}
	}

	expectToken("}");
	config.servers.push_back(server);
}

void ConfigParser::parseLocationBlock(ServerConfig& server) {
	// location の後のパスを取得
	std::string path = nextToken();
	expectToken("{");

	LocationConfig location;
	location.path = path;

	while (hasMoreTokens() && peekToken() != "}") {
		std::string directive = nextToken();

		if (directive == "root") {
			parseRootDirective(location);
		} else if (directive == "index") {
			parseIndexDirective(location);
		} else if (directive == "autoindex") {
			parseAutoindexDirective(location);
		} else if (directive == "allowed_methods") {
			parseAllowedMethodsDirective(location);
		} else if (directive == "upload_path") {
			parseUploadPathDirective(location);
		} else if (directive == "cgi_extension") {
			parseCgiExtensionDirective(location);
		} else if (directive == "cgi_path") {
			parseCgiPathDirective(location);
		} else {
			throw std::runtime_error("Unknown directive in location block: " + directive);
		}
	}

	expectToken("}");
	server.locations.push_back(location);
}

// ディレクティブのパース実装

void ConfigParser::parseListenDirective(ServerConfig& server) {
	std::string value = nextToken();
	server.listen.push_back(value);
	expectToken(";");
}

void ConfigParser::parseServerNameDirective(ServerConfig& server) {
	server.serverName = nextToken();
	expectToken(";");
}

void ConfigParser::parseErrorPageDirective(ServerConfig& server) {
	std::string statusStr = nextToken();
	std::string path;

	// 複数のステータスコードに対応
	std::vector<int> statusCodes;
	while (hasMoreTokens() && peekToken() != ";") {
		std::string token = peekToken();
		if (isNumber(token)) {
			statusCodes.push_back(std::atoi(nextToken().c_str()));
		} else {
			path = nextToken();
			break;
		}
	}

	// 最初のトークンもステータスコードの場合
	if (isNumber(statusStr)) {
		statusCodes.insert(statusCodes.begin(), std::atoi(statusStr.c_str()));
	}

	// 各ステータスコードに同じパスを割り当て
	for (size_t i = 0; i < statusCodes.size(); ++i) {
		server.errorPages[statusCodes[i]] = path;
	}

	expectToken(";");
}

void ConfigParser::parseClientMaxBodySizeDirective(ServerConfig& server) {
	std::string sizeStr = nextToken();
	server.clientMaxBodySize = parseSize(sizeStr);
	expectToken(";");
}

void ConfigParser::parseRootDirective(LocationConfig& location) {
	location.root = nextToken();
	expectToken(";");
}

void ConfigParser::parseIndexDirective(LocationConfig& location) {
	location.index = nextToken();
	expectToken(";");
}

void ConfigParser::parseAutoindexDirective(LocationConfig& location) {
	std::string value = nextToken();
	location.autoindex = (value == "on" || value == "true");
	expectToken(";");
}

void ConfigParser::parseAllowedMethodsDirective(LocationConfig& location) {
	while (hasMoreTokens() && peekToken() != ";") {
		location.allowedMethods.push_back(nextToken());
	}
	expectToken(";");
}

void ConfigParser::parseUploadPathDirective(LocationConfig& location) {
	location.uploadPath = nextToken();
	expectToken(";");
}

void ConfigParser::parseCgiExtensionDirective(LocationConfig& location) {
	location.cgiExtension = nextToken();
	expectToken(";");
}

void ConfigParser::parseCgiPathDirective(LocationConfig& location) {
	location.cgiPath = nextToken();
	expectToken(";");
}

// ヘルパー関数の実装

std::string ConfigParser::nextToken() {
	if (!hasMoreTokens()) {
		throw std::runtime_error("Unexpected end of config file");
	}
	return _tokens[_currentTokenIndex++];
}

std::string ConfigParser::peekToken() const {
	if (!hasMoreTokens()) {
		throw std::runtime_error("Unexpected end of config file");
	}
	return _tokens[_currentTokenIndex];
}

bool ConfigParser::hasMoreTokens() const {
	return _currentTokenIndex < _tokens.size();
}

void ConfigParser::expectToken(const std::string& expected) {
	std::string token = nextToken();
	if (token != expected) {
		throw std::runtime_error("Expected '" + expected + "' but got '" + token + "'");
	}
}

void ConfigParser::skipSemicolon() {
	if (hasMoreTokens() && peekToken() == ";") {
		nextToken();
	}
}

size_t ConfigParser::parseSize(const std::string& sizeStr) {
	if (sizeStr.empty()) {
		throw std::runtime_error("Empty size string");
	}

	std::string numPart;
	char unit = '\0';

	// 数値部分と単位部分を分離
	for (size_t i = 0; i < sizeStr.length(); ++i) {
		if (std::isdigit(sizeStr[i])) {
			numPart += sizeStr[i];
		} else {
			unit = sizeStr[i];
			break;
		}
	}

	size_t base = std::atol(numPart.c_str());

	// 単位に応じて変換
	switch (unit) {
		case 'K':
		case 'k':
			return base * 1024;
		case 'M':
		case 'm':
			return base * 1024 * 1024;
		case 'G':
		case 'g':
			return base * 1024 * 1024 * 1024;
		case '\0':
			return base;
		default:
			throw std::runtime_error("Unknown size unit: " + std::string(1, unit));
	}
}

bool ConfigParser::isNumber(const std::string& str) {
	if (str.empty()) {
		return false;
	}
	for (size_t i = 0; i < str.length(); ++i) {
		if (!std::isdigit(str[i])) {
			return false;
		}
	}
	return true;
}
