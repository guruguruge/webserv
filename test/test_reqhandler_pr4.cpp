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
    // ルート
    mkdir("test_www", 0755);
    createFile("test_www/index.html", "<html>Index</html>");

    // 1. AutoIndex ON 用ディレクトリ
    mkdir("test_www/auto_on", 0755);
    createFile("test_www/auto_on/file1.txt", "Content 1");
    createFile("test_www/auto_on/file2.txt", "Content 2");
    mkdir("test_www/auto_on/sub_dir", 0755);

    // 2. AutoIndex OFF 用ディレクトリ (Indexファイルなし)
    mkdir("test_www/auto_off", 0755);
    createFile("test_www/auto_off/hidden.txt", "Hidden Content");
  }

  static void teardown() {
    unlink("test_www/auto_off/hidden.txt");
    rmdir("test_www/auto_off");

    rmdir("test_www/auto_on/sub_dir");
    unlink("test_www/auto_on/file2.txt");
    unlink("test_www/auto_on/file1.txt");
    rmdir("test_www/auto_on");

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

  // Location / (通常)
  LocationConfig locRoot;
  locRoot.path = "/";
  locRoot.root = "./test_www";
  locRoot.index = "index.html";
  server.locations.push_back(locRoot);

  // Location /auto_on/ (AutoIndex: ON)
  LocationConfig locAutoOn;
  locAutoOn.path = "/auto_on/";
  locAutoOn.root = "./test_www";
  locAutoOn.autoindex = true;
  server.locations.push_back(locAutoOn);

  // Location /auto_off/ (AutoIndex: OFF)
  LocationConfig locAutoOff;
  locAutoOff.path = "/auto_off/";
  locAutoOff.root = "./test_www";
  locAutoOff.autoindex = false;
  server.locations.push_back(locAutoOff);

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
                    const std::vector<std::string>& expectedBodyContents =
                        std::vector<std::string>()) {
  client.res.build();

  // レスポンスデータを読み出す
  std::string fullResponse;
  // 簡易的に全データを取得するループ (本来は送信処理が必要だがテスト用)
  while (true) {
    const char* data = client.res.getData();
    size_t size = client.res.getRemainingSize();
    if (data && size > 0) {
      fullResponse.append(data, size);
      client.res.advance(size);
    } else {
      if (client.res.isDone() || client.res.isError())
        break;
      client.res.advance(0);
    }
  }

  std::string expectedStatusStr = " " + toString(expectedStatus) + " ";
  bool statusMatch =
      (fullResponse.find(expectedStatusStr) != std::string::npos);

  bool bodyMatch = true;
  std::string missingContent;
  for (size_t i = 0; i < expectedBodyContents.size(); ++i) {
    if (fullResponse.find(expectedBodyContents[i]) == std::string::npos) {
      bodyMatch = false;
      missingContent = expectedBodyContents[i];
      break;
    }
  }

  if (statusMatch && bodyMatch) {
    std::cout << GREEN << "[PASS] " << testName << RESET << std::endl;
  } else {
    std::string statusLine = fullResponse.substr(0, fullResponse.find("\r\n"));
    std::cout << RED << "[FAIL] " << testName
              << " | Expected: " << expectedStatus;
    if (!bodyMatch)
      std::cout << ", Missing Body Content: '" << missingContent << "'";
    std::cout << ", Actual Status: " << statusLine << RESET << std::endl;
  }
}

// =============================================================================
// Main
// =============================================================================
int main() {
  std::cout << "Starting Issue 4 (AutoIndex) Tests..." << std::endl;

  TestEnvironment::setup();
  MainConfig config;
  setupTestConfig(config);
  RequestHandler handler(config);
  Client client(999, 8080, "127.0.0.1", NULL);

  // --- TEST 1: AutoIndex ON ---
  // ディレクトリにアクセス -> ファイル一覧が返ってくるはず
  {
    setupClientRequest(client, "GET", "/auto_on/");
    handler.handle(&client);
    std::vector<std::string> expected;
    expected.push_back("file1.txt");
    expected.push_back("file2.txt");
    expected.push_back("sub_dir/");
    expected.push_back("Index of /auto_on/");
    assertResponse(client, 200, "GET /auto_on/ (AutoIndex ON)", expected);
  }

  // --- TEST 2: AutoIndex OFF ---
  // Indexファイルがないディレクトリにアクセス -> 403 Forbidden が返ってくるはず
  {
    setupClientRequest(client, "GET", "/auto_off/");
    handler.handle(&client);
    assertResponse(client, 403, "GET /auto_off/ (AutoIndex OFF)");
  }

  // --- TEST 3: 通常ファイル (AutoIndexディレクトリ内) ---
  // AutoIndexがONでも、ファイルを指定すれば中身が返ってくるはず
  {
    setupClientRequest(client, "GET", "/auto_on/file1.txt");
    handler.handle(&client);
    std::vector<std::string> expected;
    expected.push_back("Content 1");
    assertResponse(client, 200, "GET /auto_on/file1.txt (Normal File)",
                   expected);
  }

  // --- TEST 4: 通常Indexファイル優先 ---
  // ルートディレクトリ (AutoIndex設定なし、Indexあり) -> index.html が返ってくるはず
  {
    setupClientRequest(client, "GET", "/");
    handler.handle(&client);
    std::vector<std::string> expected;
    expected.push_back("<html>Index</html>");
    assertResponse(client, 200, "GET / (Index File Priority)", expected);
  }

  TestEnvironment::teardown();
  std::cout << "All tests finished." << std::endl;
  return 0;
}