#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "Client.hpp"
#include "Config.hpp"
#include "Http.hpp"
#include "RequestHandler.hpp"

// 色付き出力用マクロ
#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"
#define YELLOW "\033[33m"

// 数値を文字列に変換するヘルパー関数
template <typename T>
std::string toString(const T& value) {
  std::ostringstream oss;
  oss << value;
  return oss.str();
}

// =============================================================================
// テスト環境のセットアップ
// =============================================================================
class TestEnvironment {
 public:
  static void setup() {
    mkdir("test_www", 0755);
    createFile("test_www/index.html", "<html>Index</html>");

    // カスタムエラーページ用
    createFile("test_www/404.html", "<html>Custom 404 Error Page</html>");

    // 権限なしファイル
    createFile("test_www/secret.txt", "secret");
    chmod("test_www/secret.txt", 0000);
  }

  static void teardown() {
    chmod("test_www/secret.txt", 0644);
    unlink("test_www/secret.txt");
    unlink("test_www/404.html");
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
// Configセットアップ
// =============================================================================
void setupTestConfig(MainConfig& config) {
  ServerConfig server;
  server.listen_port = 8080;
  server.server_names.push_back("localhost");
  server.root = "./test_www";

  // エラーページ設定: 404 -> /404.html
  server.error_pages[404] = "/404.html";

  // Location / (通常)
  LocationConfig locRoot;
  locRoot.path = "/";
  locRoot.root = "./test_www";
  locRoot.index = "index.html";
  server.locations.push_back(locRoot);

  // Location /old-page (リダイレクト)
  LocationConfig locRedirect;
  locRedirect.path = "/old-page";
  locRedirect.return_redirect =
      std::make_pair(301, "http://localhost:8080/new-page");
  server.locations.push_back(locRedirect);

  config.servers.push_back(server);
}

// =============================================================================
// ヘルパー
// =============================================================================
void setupClientRequest(Client& client, const std::string& method,
                        const std::string& path) {
  client.req.clear();
  client.res.clear();
  std::string rawRequest = method + " " + path + " HTTP/1.1\r\n";
  rawRequest += "Host: localhost:8080\r\n\r\n";
  client.req.feed(rawRequest.c_str(), rawRequest.length());
}

void assertResponse(Client& client, int expectedStatus,
                    const std::string& testName,
                    const std::string& expectedBodyContent = "") {
  client.res.build();

  // レスポンスデータを最後まで読み出すシミュレーション
  std::string fullResponse;
  while (true) {
    const char* data = client.res.getData();
    size_t size = client.res.getRemainingSize();

    if (data && size > 0) {
      fullResponse.append(data, size);
      // 送信完了したことにする（次のチャンクやファイル読み込みを進める）
      client.res.advance(size);
    } else {
      if (client.res.isDone()) {
        break;
      }
      if (client.res.isError()) {
        std::cout << RED << "[ERROR] Response build error: "
                  << client.res.getErrorMessage() << RESET << std::endl;
        break;
      }
      // データがまだ来ていない(バッファが空の)場合、読み込みを進めるために0バイト送信扱いでadvanceを呼ぶ
      client.res.advance(0);
    }
  }

  std::string expectedStatusStr = " " + toString(expectedStatus) + " ";

  bool statusMatch =
      (fullResponse.find(expectedStatusStr) != std::string::npos);
  bool bodyMatch = true;

  if (!expectedBodyContent.empty()) {
    bodyMatch = (fullResponse.find(expectedBodyContent) != std::string::npos);
  }

  if (statusMatch && bodyMatch) {
    std::cout << GREEN << "[PASS] " << testName << RESET << std::endl;
  } else {
    std::string statusLine = fullResponse.substr(0, fullResponse.find("\r\n"));
    std::cout << RED << "[FAIL] " << testName
              << " | Expected: " << expectedStatus;
    if (!expectedBodyContent.empty())
      std::cout << " with body containing '" << expectedBodyContent << "'";
    std::cout << ", Actual Status Line: " << statusLine << RESET << std::endl;

    if (statusMatch && !bodyMatch) {
      std::cout << "      Received Body (Tail): "
                << fullResponse.substr(
                       std::min(fullResponse.size(), (size_t)50))
                << "..." << std::endl;
    }
  }
}

// =============================================================================
// Main
// =============================================================================
int main() {
  std::cout << "Starting Issue 3 Tests..." << std::endl;

  TestEnvironment::setup();
  MainConfig config;
  setupTestConfig(config);
  RequestHandler handler(config);
  Client client(999, 8080, "127.0.0.1", NULL);

  // --- TEST 1: 正常系 ---
  setupClientRequest(client, "GET", "/index.html");
  handler.handle(&client);
  assertResponse(client, 200, "GET /index.html (Normal)");

  // --- TEST 2: リダイレクト (return 301) ---
  setupClientRequest(client, "GET", "/old-page");
  handler.handle(&client);
  assertResponse(client, 301, "GET /old-page (Redirect 301)");
  // Locationヘッダーの確認は簡易的にスキップするか、必要なら実装

  // --- TEST 3: カスタムエラーページ (404 -> /404.html) ---
  // 存在しないページにアクセス -> 内部リダイレクトで /404.html の内容が返るはず
  // ただしステータスコードは 404 のまま
  setupClientRequest(client, "GET", "/nothing");
  handler.handle(&client);
  assertResponse(client, 404, "GET /nothing (Custom 404 Page)",
                 "Custom 404 Error Page");

  // --- TEST 4: デフォルトエラーページ (403 Forbidden) ---
  // 403のエラーページ設定はないので、デフォルトHTMLが返るはず
  if (access("test_www/secret.txt", R_OK) != 0) {
    setupClientRequest(client, "GET", "/secret.txt");
    handler.handle(&client);
    assertResponse(client, 403, "GET /secret.txt (Default 403 Page)");
  } else {
    std::cout << YELLOW << "[SKIP] TEST 4 (Cannot trigger 403)" << RESET
              << std::endl;
  }

  TestEnvironment::teardown();
  std::cout << "All tests finished." << std::endl;
  return 0;
}