#include <iostream>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <cstdlib>

static int connectToServer(const char* host, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "socket() failed: " << strerror(errno) << std::endl;
        return -1;
    }
    
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    addr.sin_addr.s_addr = inet_addr(host);
    
    if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "connect() failed: " << strerror(errno) << std::endl;
        close(sock);
        return -1;
    }
    
    return sock;
}

static bool sendRequest(int sock, const std::string& request) {
    size_t total_sent = 0;
    while (total_sent < request.size()) {
        ssize_t sent = send(sock, request.c_str() + total_sent, 
                           request.size() - total_sent, 0);
        if (sent <= 0) {
            std::cerr << "send() failed: " << strerror(errno) << std::endl;
            return false;
        }
        total_sent += sent;
    }
    return true;
}

static std::string receiveResponse(int sock) {
    char buffer[4096];
    std::string response;
    
    // タイムアウト設定 (5秒)
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    while (true) {
        ssize_t received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::cerr << "recv() timeout after 5 seconds\n";
            } else {
                std::cerr << "recv() failed: " << strerror(errno) << std::endl;
            }
            break;
        }
        if (received == 0) {
            break;  // 接続が閉じられた
        }
        buffer[received] = '\0';
        response.append(buffer, received);
        
        // レスポンスヘッダーが完了したらボディサイズを確認
        if (response.find("\r\n\r\n") != std::string::npos) {
            // 簡易チェック: Content-Length があればそれに基づいて終了判定
            // ここでは簡易的に実装（実際はContent-Lengthをパースすべき）
            break;
        }
    }
    
    return response;
}

// =============================================================================
// テスト1: 巨大なヘッダー（MAX_HEADER_SIZE超過）
// =============================================================================
static void testLargeHeaders(const char* host, int port) {
    std::cout << "\n=== Test 1: Large Headers (> MAX_HEADER_SIZE) ===" << std::endl;
    
    int sock = connectToServer(host, port);
    if (sock < 0) {
        std::cout << "✗ FAILED: Connection failed\n";
        return;
    }
    
    // 20KB のヘッダー（MAX_HEADER_SIZE = 16384を超過）
    std::ostringstream oss;
    oss << "GET / HTTP/1.1\r\n";
    oss << "Host: localhost\r\n";
    
    // 巨大なヘッダーを追加 (200個のヘッダー、各100バイト = 20KB以上)
    for (int i = 0; i < 200; ++i) {
        oss << "X-Custom-Header-" << i << ": ";
        // 各ヘッダーに100バイトの値を追加
        for (int j = 0; j < 100; ++j) {
            oss << "A";
        }
        oss << "\r\n";
    }
    oss << "\r\n";
    
    std::string request = oss.str();
    std::cout << "Request size: " << request.size() << " bytes\n";
    
    if (!sendRequest(sock, request)) {
        std::cout << "✗ FAILED: Send failed\n";
        close(sock);
        return;
    }
    
    std::string response = receiveResponse(sock);
    close(sock);
    
    // 431 (Request Header Fields Too Large) または 400 を期待
    if (response.find("431") != std::string::npos || 
        response.find("400") != std::string::npos) {
        std::cout << "✓ PASSED: Server rejected large headers\n";
    } else {
        std::cout << "✗ FAILED: Expected 431 or 400, got:\n" << response.substr(0, 100) << "...\n";
    }
}

// =============================================================================
// テスト2: 巨大なボディ（client_max_body_size超過）
// =============================================================================
static void testLargeBody(const char* host, int port) {
    std::cout << "\n=== Test 2: Large Body (> client_max_body_size) ===" << std::endl;
    
    int sock = connectToServer(host, port);
    if (sock < 0) {
        std::cout << "✗ FAILED: Connection failed\n";
        return;
    }
    
    // client_max_body_size のデフォルト値を超える (1MB + 1KB)
    const size_t body_size = 1024 * 1024 + 1024;
    
    std::ostringstream oss;
    oss << "POST /upload HTTP/1.1\r\n";
    oss << "Host: localhost\r\n";
    oss << "Content-Length: " << body_size << "\r\n";
    oss << "Content-Type: application/octet-stream\r\n";
    oss << "\r\n";
    
    std::string header = oss.str();
    
    if (!sendRequest(sock, header)) {
        std::cout << "✗ FAILED: Send header failed\n";
        close(sock);
        return;
    }
    
    // ボディを送信（分割して送る）
    const size_t chunk_size = 4096;
    char* body_chunk = new char[chunk_size];
    std::memset(body_chunk, 'A', chunk_size);
    
    size_t sent_total = 0;
    bool send_success = true;
    
    while (sent_total < body_size) {
        size_t to_send = (body_size - sent_total > chunk_size) ? chunk_size : (body_size - sent_total);
        ssize_t sent = send(sock, body_chunk, to_send, 0);
        if (sent <= 0) {
            send_success = false;
            break;
        }
        sent_total += sent;
    }
    
    delete[] body_chunk;
    
    if (!send_success) {
        std::cout << "✗ FAILED: Send body failed\n";
        close(sock);
        return;
    }
    
    std::string response = receiveResponse(sock);
    close(sock);
    
    // 413 (Payload Too Large) を期待
    if (response.find("413") != std::string::npos) {
        std::cout << "✓ PASSED: Server rejected large body with 413\n";
    } else {
        std::cout << "✗ FAILED: Expected 413, got:\n" << response.substr(0, 100) << "...\n";
    }
}

// =============================================================================
// テスト3: 巨大なチャンクボディ
// =============================================================================
static void testLargeChunkedBody(const char* host, int port) {
    std::cout << "\n=== Test 3: Large Chunked Body (> client_max_body_size) ===" << std::endl;
    
    int sock = connectToServer(host, port);
    if (sock < 0) {
        std::cout << "✗ FAILED: Connection failed\n";
        return;
    }
    
    std::ostringstream oss;
    oss << "POST /upload HTTP/1.1\r\n";
    oss << "Host: localhost\r\n";
    oss << "Transfer-Encoding: chunked\r\n";
    oss << "\r\n";
    
    if (!sendRequest(sock, oss.str())) {
        std::cout << "✗ FAILED: Send header failed\n";
        close(sock);
        return;
    }
    
    // 1MB + 1KB のチャンクデータを送信
    const size_t total_size = 1024 * 1024 + 1024;
    const size_t chunk_size = 4096;
    size_t sent_total = 0;
    
    while (sent_total < total_size) {
        // チャンクサイズを16進数で送信
        std::ostringstream chunk_header;
        chunk_header << std::hex << chunk_size << "\r\n";
        
        if (!sendRequest(sock, chunk_header.str())) {
            break;
        }
        
        // チャンクデータ
        std::string chunk_data(chunk_size, 'A');
        if (!sendRequest(sock, chunk_data)) {
            break;
        }
        
        // チャンク終端の CRLF
        if (!sendRequest(sock, "\r\n")) {
            break;
        }
        
        sent_total += chunk_size;
    }
    
    // 終端チャンク
    sendRequest(sock, "0\r\n\r\n");
    
    std::string response = receiveResponse(sock);
    close(sock);
    
    // 413 (Payload Too Large) を期待
    if (response.find("413") != std::string::npos) {
        std::cout << "✓ PASSED: Server rejected large chunked body with 413\n";
    } else {
        std::cout << "✗ FAILED: Expected 413, got:\n" << response.substr(0, 100) << "...\n";
    }
}

// =============================================================================
// テスト4: 過剰なヘッダー行数
// =============================================================================
static void testTooManyHeaders(const char* host, int port) {
    std::cout << "\n=== Test 4: Too Many Header Lines (> MAX_HEADER_COUNT) ===" << std::endl;
    
    int sock = connectToServer(host, port);
    if (sock < 0) {
        std::cout << "✗ FAILED: Connection failed\n";
        return;
    }
    
    std::ostringstream oss;
    oss << "GET / HTTP/1.1\r\n";
    oss << "Host: localhost\r\n";
    
    // 150個のヘッダー（MAX_HEADER_COUNT = 100を超過）
    for (int i = 0; i < 150; ++i) {
        oss << "X-Header-" << i << ": value\r\n";
    }
    oss << "\r\n";
    
    if (!sendRequest(sock, oss.str())) {
        std::cout << "✗ FAILED: Send failed\n";
        close(sock);
        return;
    }
    
    std::string response = receiveResponse(sock);
    close(sock);
    
    // 431 または 400 を期待
    if (response.find("431") != std::string::npos || 
        response.find("400") != std::string::npos) {
        std::cout << "✓ PASSED: Server rejected too many headers\n";
    } else {
        std::cout << "✗ FAILED: Expected 431 or 400, got:\n" << response.substr(0, 100) << "...\n";
    }
}

// =============================================================================
// Main
// =============================================================================
int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <host> <port>" << std::endl;
        return 1;
    }
    
    const char* host = argv[1];
    int port = std::atoi(argv[2]);
    
    std::cout << "========================================" << std::endl;
    std::cout << " Large Request Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Target: " << host << ":" << port << std::endl;
    
    testLargeHeaders(host, port);
    testLargeBody(host, port);
    testLargeChunkedBody(host, port);
    testTooManyHeaders(host, port);
    
    std::cout << "\n========================================" << std::endl;
    std::cout << " Tests Complete" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}