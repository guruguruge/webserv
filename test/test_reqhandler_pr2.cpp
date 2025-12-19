#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <sstream> // std::stringstream

#include "RequestHandler.hpp"
#include "Client.hpp"
#include "Config.hpp"
#include "Http.hpp"

// 色付き出力用マクロ
#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"
#define YELLOW "\033[33m"

// C++98対応: 数値を文字列に変換するヘルパー関数
template <typename T>
std::string toString(const T& value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

// =============================================================================
// テスト環境のセットアップ・ティアダウン（ファイル作成・削除）
// =============================================================================
class TestEnvironment {
public:
    static void setup() {
        // テスト用ルートディレクトリ
        mkdir("test_www", 0755);
        
        // 1. 通常ファイル
        createFile("test_www/index.html", "<html><body>Hello World</body></html>");
        
        // 2. Alias用ディレクトリとファイル
        mkdir("test_www/images", 0755);
        createFile("test_www/images/photo.jpg", "dummy image content");

        // 3. 権限なしファイル
        createFile("test_www/secret.txt", "secret");
        chmod("test_www/secret.txt", 0000); // 読み取り権限なし
    }

    static void teardown() {
        // 権限を戻さないと削除できない場合があるため復元
        chmod("test_www/secret.txt", 0644);
        unlink("test_www/secret.txt");
        unlink("test_www/images/photo.jpg");
        rmdir("test_www/images");
        unlink("test_www/index.html");
        rmdir("test_www");
    }

    static void createFile(const std::string& path, const std::string& content) {
        std::ofstream ofs(path.c_str());
        ofs << content;
        ofs.close();
    }
};

// =============================================================================
// ヘルパー関数
// =============================================================================

// テスト用のConfigを作成 (参照渡しで設定を行うように変更)
void setupTestConfig(MainConfig& config) {
    ServerConfig server;
    server.listen_port = 8080;
    server.server_names.push_back("localhost");
    server.root = "./test_www"; // デフォルトルート

    // Location / (通常)
    LocationConfig locRoot;
    locRoot.path = "/";
    locRoot.root = "./test_www";
    locRoot.index = "index.html";
    server.locations.push_back(locRoot);

    // Location /img/ (Alias動作確認)
    LocationConfig locImg;
    locImg.path = "/img/";
    locImg.alias = "./test_www/images/";
    server.locations.push_back(locImg);

    config.servers.push_back(server);
}

// Clientにリクエストデータを注入する
void setupClientRequest(Client& client, const std::string& method, const std::string& path) {
    client.req.clear(); // 状態リセット
    client.res.clear(); // レスポンスリセット
    
    // 生のHTTPリクエスト文字列を作成してパースさせる
    std::string rawRequest = method + " " + path + " HTTP/1.1\r\n";
    rawRequest += "Host: localhost:8080\r\n";
    rawRequest += "\r\n";

    client.req.feed(rawRequest.c_str(), rawRequest.length());
}

// レスポンス結果の検証
void assertResponse(Client& client, int expectedStatus, const std::string& testName) {
    // レスポンス構築（ヘッダー生成など）
    client.res.build();
    
    // 構築された生データを取得して確認
    std::string resData = client.res.getData();
    // std::to_string -> toString に変更
    std::string expectedStr = " " + toString(expectedStatus) + " "; 

    if (resData.find(expectedStr) != std::string::npos) {
        std::cout << GREEN << "[PASS] " << testName << RESET << std::endl;
    } else {
        // ステータス行だけ切り出して表示
        size_t crlf = resData.find("\r\n");
        std::string statusLine = (crlf != std::string::npos) ? resData.substr(0, crlf) : "Invalid Response";
        std::cout << RED << "[FAIL] " << testName 
                  << " | Expected: " << expectedStatus 
                  << ", Actual Status Line: " << statusLine << RESET << std::endl;
    }
}

// =============================================================================
// Main Test
// =============================================================================
int main() {
    std::cout << "Starting RequestHandler Tests..." << std::endl;

    // 1. 環境構築
    TestEnvironment::setup();
    
    // Configのセットアップ (コピーコンストラクタ問題を回避)
    MainConfig config;
    setupTestConfig(config);
    
    RequestHandler handler(config);

    // ダミーClient作成 (FD: 999, Port: 8080)
    // 4番目の引数(EpollUtils*)にNULLを渡してコンパイルエラーを回避
    Client client(999, 8080, "127.0.0.1", NULL);

    // --- TEST 1: 正常系 GET /index.html ---
    setupClientRequest(client, "GET", "/index.html");
    handler.handle(&client);
    assertResponse(client, 200, "GET /index.html (File Exist)");

    // --- TEST 2: ディレクトリインデックス GET / ---
    setupClientRequest(client, "GET", "/");
    handler.handle(&client);
    assertResponse(client, 200, "GET / (Directory Index -> index.html)");

    // --- TEST 3: Alias解決 GET /img/photo.jpg ---
    // URI: /img/photo.jpg -> Alias: ./test_www/images/photo.jpg
    setupClientRequest(client, "GET", "/img/photo.jpg");
    handler.handle(&client);
    assertResponse(client, 200, "GET /img/photo.jpg (Alias Check)");

    // --- TEST 4: 存在しないファイル (404) ---
    setupClientRequest(client, "GET", "/not_found.html");
    handler.handle(&client);
    assertResponse(client, 404, "GET /not_found.html (404 Not Found)");

    // --- TEST 5: 権限なしファイル (403) ---
    // rootユーザーやDockerのマウント設定によっては chmod 0000 が効かず、読めてしまう場合がある。
    // そのため、実際に読み取り権限があるかをチェックし、ある(読めてしまう)場合はテストをスキップする。
    if (access("test_www/secret.txt", R_OK) == 0) {
        std::cout << YELLOW << "[SKIP] GET /secret.txt (Skipping 403 test: Unable to remove read permission in this environment)" << RESET << std::endl;
    } else {
        setupClientRequest(client, "GET", "/secret.txt");
        handler.handle(&client);
        assertResponse(client, 403, "GET /secret.txt (403 Forbidden)");
    }

    // --- TEST 6: パストラバーサル攻撃 (404) ---
    // /../../etc/passwd -> ./test_www/etc/passwd に正規化されて 404 になるべき
    // (ルートディレクトリより上には行かせない)
    setupClientRequest(client, "GET", "/../../etc/passwd");
    handler.handle(&client);
    assertResponse(client, 404, "GET /../../etc/passwd (Path Traversal Prevention)");

    // 2. 後始末
    TestEnvironment::teardown();
    
    std::cout << "All tests finished." << std::endl;
    return 0;
}
