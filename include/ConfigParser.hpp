/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: webserv                                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/15                              #+#    #+#             */
/*   Updated: 2025/11/15                             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include "Config.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>

class ConfigParser {
public:
	ConfigParser(const std::string& path);
	~ConfigParser();

	Config parse();

private:
	std::string _path;
	std::vector<std::string> _tokens;
	size_t _currentTokenIndex;

	void tokenize();
	void parseServerBlock(Config& config);
	void parseLocationBlock(ServerConfig& server);

	// ヘルパー関数
	std::string nextToken();
	std::string peekToken() const;
	bool hasMoreTokens() const;
	void expectToken(const std::string& expected);
	
	// ディレクティブのパース
	void parseListenDirective(ServerConfig& server);
	void parseServerNameDirective(ServerConfig& server);
	void parseErrorPageDirective(ServerConfig& server);
	void parseClientMaxBodySizeDirective(ServerConfig& server);
	void parseRootDirective(LocationConfig& location);
	void parseIndexDirective(LocationConfig& location);
	void parseAutoindexDirective(LocationConfig& location);
	void parseAllowedMethodsDirective(LocationConfig& location);
	void parseUploadPathDirective(LocationConfig& location);
	void parseCgiExtensionDirective(LocationConfig& location);
	void parseCgiPathDirective(LocationConfig& location);

	// ユーティリティ
	size_t parseSize(const std::string& sizeStr);
	bool isNumber(const std::string& str);
	void skipSemicolon();
};

#endif
