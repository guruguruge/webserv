/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: webserv                                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/15                              #+#    #+#             */
/*   Updated: 2025/11/15                             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ConfigParser.hpp"
#include <iostream>
#include <string>

static void printConfig(const Config& config) {
	std::cout << "\n=== Configuration ===" << std::endl;
	
	for (size_t i = 0; i < config.servers.size(); ++i) {
		const ServerConfig& server = config.servers[i];
		std::cout << "\n[Server " << i + 1 << "]" << std::endl;
		
		std::cout << "  Listen: ";
		for (size_t j = 0; j < server.listen.size(); ++j) {
			std::cout << server.listen[j];
			if (j < server.listen.size() - 1) std::cout << ", ";
		}
		std::cout << std::endl;
		
		if (!server.serverName.empty()) {
			std::cout << "  Server Name: " << server.serverName << std::endl;
		}
		
		std::cout << "  Client Max Body Size: " << server.clientMaxBodySize << " bytes" << std::endl;
		
		if (!server.errorPages.empty()) {
			std::cout << "  Error Pages:" << std::endl;
			for (std::map<int, std::string>::const_iterator it = server.errorPages.begin();
			     it != server.errorPages.end(); ++it) {
				std::cout << "    " << it->first << " -> " << it->second << std::endl;
			}
		}
		
		std::cout << "  Locations (" << server.locations.size() << "):" << std::endl;
		for (size_t j = 0; j < server.locations.size(); ++j) {
			const LocationConfig& loc = server.locations[j];
			std::cout << "    [" << loc.path << "]" << std::endl;
			
			if (!loc.root.empty()) {
				std::cout << "      Root: " << loc.root << std::endl;
			}
			if (!loc.index.empty()) {
				std::cout << "      Index: " << loc.index << std::endl;
			}
			std::cout << "      Autoindex: " << (loc.autoindex ? "on" : "off") << std::endl;
			
			if (!loc.allowedMethods.empty()) {
				std::cout << "      Allowed Methods: ";
				for (size_t k = 0; k < loc.allowedMethods.size(); ++k) {
					std::cout << loc.allowedMethods[k];
					if (k < loc.allowedMethods.size() - 1) std::cout << ", ";
				}
				std::cout << std::endl;
			}
			
			if (!loc.uploadPath.empty()) {
				std::cout << "      Upload Path: " << loc.uploadPath << std::endl;
			}
			if (!loc.cgiExtension.empty()) {
				std::cout << "      CGI Extension: " << loc.cgiExtension << std::endl;
			}
			if (!loc.cgiPath.empty()) {
				std::cout << "      CGI Path: " << loc.cgiPath << std::endl;
			}
		}
	}
	std::cout << "\n=====================\n" << std::endl;
}

int main(int argc, char **argv)
{
	std::string configPath;

	// デフォルトの設定ファイルパス
	if (argc == 1) {
		configPath = "config/default.conf";
	} else if (argc == 2) {
		configPath = argv[1];
	} else {
		std::cerr << "Usage: " << argv[0] << " [config_file]" << std::endl;
		return 1;
	}

	try {
		std::cout << "Starting webserv..." << std::endl;
		std::cout << "Config file: " << configPath << std::endl;

		// Step 1: 設定ファイルのパース
		ConfigParser parser(configPath);
		Config config = parser.parse();
		
		std::cout << "✅ Configuration parsed successfully!" << std::endl;
		printConfig(config);

		// TODO: Step 2 - リスナーソケットの作成
		// TODO: Step 3 - イベントループの開始

		std::cout << "webserv initialized successfully (TODO: implement server logic)" << std::endl;

	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
