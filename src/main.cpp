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

#include <iostream>
#include <string>

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

	std::cout << "Starting webserv..." << std::endl;
	std::cout << "Config file: " << configPath << std::endl;

	// TODO: Step 1 - 設定ファイルのパース
	// TODO: Step 2 - リスナーソケットの作成
	// TODO: Step 3 - イベントループの開始

	std::cout << "webserv initialized successfully (TODO: implement server logic)" << std::endl;

	return 0;
}
