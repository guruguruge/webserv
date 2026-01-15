#include <iostream>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <sys/time.h>

static double getTimeInSeconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <host> <port>" << std::endl;
        return 1;
    }

    const char* host = argv[1];
    int port = std::atoi(argv[2]);
    std::vector<int> sockets;
    
    std::cout << "========================================" << std::endl;
    std::cout << " FD Limit Stress Test" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Target: " << host << ":" << port << std::endl;
    
    double start_time = getTimeInSeconds();
    
    // 接続を開き続ける
    for (int i = 0; i < 10000; ++i) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            std::cerr << "\n[ERROR] socket() failed at iteration " << i 
                      << ": " << strerror(errno) << std::endl;
            break;
        }
        
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(port));
        addr.sin_addr.s_addr = inet_addr(host);
        
        if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), 
                    sizeof(addr)) < 0) {
            std::cerr << "\n[ERROR] connect() failed at iteration " << i 
                      << ": " << strerror(errno) << std::endl;
            close(sock);
            break;
        }
        
        sockets.push_back(sock);
        
        if ((i + 1) % 100 == 0) {
            std::cout << "✓ Opened " << (i + 1) << " connections" << std::endl;
        }
    }
    
    double elapsed = getTimeInSeconds() - start_time;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << " Test Results" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Total connections: " << sockets.size() << std::endl;
    std::cout << "Time elapsed: " << elapsed << " seconds" << std::endl;
    std::cout << "Connections/sec: " << (sockets.size() / elapsed) << std::endl;
    std::cout << "\nPress Enter to close all connections..." << std::endl;
    std::cin.get();
    
    // クリーンアップ
    std::cout << "Closing all connections..." << std::endl;
    for (std::vector<int>::iterator it = sockets.begin(); 
         it != sockets.end(); ++it) {
        close(*it);
    }
    
    std::cout << "✓ All connections closed successfully" << std::endl;
    return 0;
}