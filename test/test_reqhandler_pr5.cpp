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

// ファイルの中身を読み込むヘルパー
std::string readFile(const std::string& path) {
  std::ifstream ifs(path.c_str(), std::ios::binary);
  if (!ifs)
    return "";
  std::stringstream buffer;
  buffer << ifs.rdbuf();
  return buffer.str();
}

// =============================================================================
// テスト環境のセットアップ
// =============================================================================
class TestEnvironment {
 public:
  static void setup() {
    mkdir("test_www", 0755);
    createFile("test_www/index.html", "Index Content");

    // POST/DELETE用ディレクトリ
    mkdir("test_www/uploads", 0755);

    // DELETEテスト用ファイル
    createFile("test_www/to_delete.txt", "Delete me");

    // 権限チェック用
    mkdir("test_www/no_write", 0555);  // 書き込み不可
    createFile("test_www/no_write/file.txt", "Read Only");
  }

  static void teardown() {
    // 権限を戻して削除
    chmod("test_www/no_write", 0755);
    unlink("test_www/no_write/file.txt");
    rmdir("test_www/no_write");

    unlink("test_www/to_delete.txt");
    unlink("test_www/uploads/new_file.txt");
    unlink("test_www/uploads/overwrite.txt");
    rmdir("test_www/uploads");
    unlink("test_www/overwrite.txt");
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

  // Location / (全許可)
  LocationConfig locRoot;
  locRoot.path = "/";
  locRoot.root = "./test_www";
  locRoot.allow_methods.push_back(GET);
  locRoot.allow_methods.push_back(POST);
  locRoot.allow_methods.push_back(DELETE);
  server.locations.push_back(locRoot);

  // Location /uploads/ (upload_path 指定)
  LocationConfig locUpload;
  locUpload.path = "/uploads/";
  locUpload.root = "./test_www";  // URL解決用
  locUpload.upload_path = "./test_www/uploads/";
  locUpload.allow_methods.push_back(GET);
  locUpload.allow_methods.push_back(POST);
  locUpload.allow_methods.push_back(DELETE);
  server.locations.push_back(locUpload);

  // Location /readonly/ (GETのみ)
  LocationConfig locReadOnly;
  locReadOnly.path = "/readonly/";
  locReadOnly.alias = "./test_www/";
  locReadOnly.allow_methods.push_back(GET);
  // POST, DELETE は追加しない
  server.locations.push_back(locReadOnly);

  config.servers.push_back(server);
}

// =============================================================================
// ヘルパー
// =============================================================================
void setupClientRequest(Client& client, const std::string& method,
                        const std::string& path, const std::string& body = "") {
  client.req.clear();
  client.res.clear();

  std::string rawRequest = method + " " + path + " HTTP/1.1\r\n";
  rawRequest += "Host: localhost:8080\r\n";
  if (!body.empty()) {
    rawRequest += "Content-Length: " + toString(body.length()) + "\r\n";
  }
  rawRequest += "\r\n";
  if (!body.empty()) {
    rawRequest += body;
  }

  client.req.feed(rawRequest.c_str(), rawRequest.length());
}

void assertResponse(Client& client, int expectedStatus,
                    const std::string& testName) {
  client.res.build();
  std::string resData = client.res.getData();
  std::string expectedStatusStr = " " + toString(expectedStatus) + " ";

  if (resData.find(expectedStatusStr) != std::string::npos) {
    std::cout << GREEN << "[PASS] " << testName << RESET << std::endl;
  } else {
    std::string statusLine = resData.substr(0, resData.find("\r\n"));
    std::cout << RED << "[FAIL] " << testName
              << " | Expected: " << expectedStatus << ", Actual: " << statusLine
              << RESET << std::endl;
  }
}

// =============================================================================
// Main
// =============================================================================
int main() {
  std::cout << "Starting Issue 5 (POST/DELETE) Tests..." << std::endl;

  TestEnvironment::setup();
  MainConfig config;
  setupTestConfig(config);
  RequestHandler handler(config);
  Client client(999, 8080, "127.0.0.1", NULL);

  // --- TEST 1: POST 新規作成 (upload_pathあり) ---
  {
    std::string content = "New File Content";
    setupClientRequest(client, "POST", "/uploads/new_file.txt", content);
    handler.handle(&client);
    assertResponse(client, 201, "POST /uploads/new_file.txt (Create)");

    // ファイルが実際にできたか確認
    std::string fileContent = readFile("test_www/uploads/new_file.txt");
    if (fileContent == content) {
      std::cout << GREEN << "       -> File content matches." << RESET
                << std::endl;
    } else {
      std::cout << RED << "       -> File content mismatch!" << RESET
                << std::endl;
    }
  }

  // --- TEST 2: POST 上書き (ルート直下) ---
  // 事前にファイル作成
  TestEnvironment::createFile("test_www/overwrite.txt", "Old Content");
  {
    std::string newContent = "Overwritten Content";
    setupClientRequest(client, "POST", "/overwrite.txt", newContent);
    handler.handle(&client);
    assertResponse(client, 201, "POST /overwrite.txt (Overwrite)");

    std::string fileContent = readFile("test_www/overwrite.txt");
    if (fileContent == newContent) {
      std::cout << GREEN << "       -> File content overwritten." << RESET
                << std::endl;
    } else {
      std::cout << RED << "       -> File content mismatch!" << RESET
                << std::endl;
    }
  }

  // --- TEST 3: DELETE 正常系 ---
  {
    setupClientRequest(client, "DELETE", "/to_delete.txt");
    handler.handle(&client);
    assertResponse(client, 204, "DELETE /to_delete.txt (Success)");

    // ファイルが消えているか確認
    if (access("test_www/to_delete.txt", F_OK) != 0) {
      std::cout << GREEN << "       -> File deleted." << RESET << std::endl;
    } else {
      std::cout << RED << "       -> File still exists!" << RESET << std::endl;
    }
  }

  // --- TEST 4: DELETE 存在しないファイル ---
  {
    setupClientRequest(client, "DELETE", "/non_existent.txt");
    handler.handle(&client);
    assertResponse(client, 404, "DELETE /non_existent.txt (404)");
  }

  // --- TEST 5: Method Not Allowed ---
  {
    // /readonly/ は GET のみ許可
    setupClientRequest(client, "DELETE", "/readonly/index.html");
    handler.handle(&client);
    assertResponse(client, 405,
                   "DELETE /readonly/index.html (405 Method Not Allowed)");
  }

  // --- TEST 6: POST ディレクトリへの書き込み (403) ---
  {
    setupClientRequest(client, "POST", "/uploads/",
                       "data");  // 末尾スラッシュ -> ディレクトリ扱い
    handler.handle(&client);
    assertResponse(client, 403, "POST /uploads/ (Directory 403)");
  }

  TestEnvironment::teardown();
  std::cout << "All tests finished." << std::endl;
  return 0;
}
