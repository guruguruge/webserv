/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Config.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: webserv                                    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/11/15                              #+#    #+#             */
/*   Updated: 2025/11/15                             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <map>

struct LocationConfig {
	std::string path;
	std::vector<std::string> allowedMethods;
	std::string root;
	std::string index;
	bool autoindex;
	std::string uploadPath;
	std::string cgiExtension;
	std::string cgiPath;

	LocationConfig();
};

struct ServerConfig {
	std::vector<std::string> listen; // "host:port" or ":port"
	std::string serverName;
	std::map<int, std::string> errorPages; // status -> file path
	size_t clientMaxBodySize;
	std::vector<LocationConfig> locations;

	ServerConfig();
};

struct Config {
	std::vector<ServerConfig> servers;

	Config();
};

#endif
