#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
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
    mkdir("test_www/cgi-bin", 0755);

    // テスト用CGIスクリプト (Shell Script)
    // ヘッダーとボディ、環境変数、標準入力の内容を出力する
    std::string scriptContent =
        "#!/bin/sh\n"
        "echo \"Content-Type: text/plain\"\n"
        "echo \"\"\n"  // ヘッダー終了
        "echo \"Hello CGI\"\n"
        "echo \"METHOD=$REQUEST_METHOD\"\n"
        "if [ \"$REQUEST_METHOD\" = \"POST\" ]; then\n"
        "  cat\n"  // 標準入力をそのまま出力
        "fi\n";

    createFile("test_www/cgi-bin/test.sh", scriptContent);
    chmod("test_www/cgi-bin/test.sh", 0755);  // 実行権限付与
  }

  static void teardown() {
    unlink("test_www/cgi-bin/test.sh");
    rmdir("test_www/cgi-bin");
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

  // Location /cgi-bin/ (CGI設定)
  LocationConfig locCgi;
  locCgi.path = "/cgi-bin/";
  locCgi.root = "./test_www";
  locCgi.cgi_extension = ".sh";

  // [修正] GETとPOSTを許可する (デフォルトでは空またはGETのみの可能性があるため明示)
  locCgi.allow_methods.push_back(GET);
  locCgi.allow_methods.push_back(POST);

  server.locations.push_back(locCgi);

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
    rawRequest += "Content-Type: text/plain\r\n";
  }
  rawRequest += "\r\n";
  if (!body.empty()) {
    rawRequest += body;
  }

  client.req.feed(rawRequest.c_str(), rawRequest.length());
}

// CGI実行結果の検証
void assertCgiExecution(Client& client, const std::string& testName,
                        const std::string& expectedOutputPart) {
  // 1. 状態遷移の確認
  if (client.getState() != WAITING_CGI) {
    std::cout << RED << "[FAIL] " << testName
              << " | Expected State: WAITING_CGI, Actual: " << client.getState()
              << RESET << std::endl;
    return;
  }

  // 2. プロセスIDとFDの確認
  if (client.getCgiPid() <= 0 || client.getCgiStdoutFd() < 0) {
    std::cout << RED << "[FAIL] " << testName
              << " | Invalid PID or FD. PID=" << client.getCgiPid() << RESET
              << std::endl;
    return;
  }

  // 3. パイプからの読み出し（CGI出力の確認）

  if (client.getCgiStdinFd() >= 0) {
    const std::vector<char>& body = client.req.getBody();
    if (!body.empty()) {
      write(client.getCgiStdinFd(), &body[0], body.size());
    }
    close(client.getCgiStdinFd());
  }

  // 出力パイプから読み出し
  char buf[1024];
  std::string output;
  int n;
  while ((n = read(client.getCgiStdoutFd(), buf, sizeof(buf) - 1)) > 0) {
    buf[n] = '\0';
    output += buf;
  }
  close(client.getCgiStdoutFd());

  // 子プロセスの終了待ち
  int status;
  waitpid(client.getCgiPid(), &status, 0);

  // 4. 結果判定
  if (output.find(expectedOutputPart) != std::string::npos) {
    std::cout << GREEN << "[PASS] " << testName << RESET << std::endl;
  } else {
    std::cout << RED << "[FAIL] " << testName
              << " | Expected output containing: '" << expectedOutputPart << "'"
              << RESET << std::endl;
    std::cout << "      Actual Output:\n" << output << std::endl;
  }
}

// =============================================================================
// Main
// =============================================================================
int main() {
  std::cout << "Starting Issue 6 (CGI) Tests..." << std::endl;

  TestEnvironment::setup();
  MainConfig config;
  setupTestConfig(config);
  RequestHandler handler(config);
  Client client(999, 8080, "127.0.0.1", NULL);

  // --- TEST 1: CGI GET ---
  {
    setupClientRequest(client, "GET", "/cgi-bin/test.sh");
    handler.handle(&client);

    // 期待値: スクリプトの標準出力に "Hello CGI" と "METHOD=GET" が含まれること
    assertCgiExecution(client, "CGI GET Execution", "METHOD=GET");
  }

  // --- TEST 2: CGI POST ---
  {
    // Clientの状態をリセット
    client.reset();

    std::string postBody = "This is POST data";
    setupClientRequest(client, "POST", "/cgi-bin/test.sh", postBody);
    handler.handle(&client);

    // 期待値: スクリプトが標準入力を読み込んで "This is POST data" を出力すること
    assertCgiExecution(client, "CGI POST Execution", postBody);
  }

  // --- TEST 3: CGI 404 (存在しないスクリプト) ---
  {
    client.reset();
    setupClientRequest(client, "GET", "/cgi-bin/not_found.sh");
    handler.handle(&client);

    // [修正] レスポンスの中身を確認するために build() を呼び出す
    client.res.build();

    // CGIではなく404エラーレスポンスになっているはず
    // 状態は WRITING_RESPONSE (エラーページ返却待ち)
    if (client.getState() == WRITING_RESPONSE && client.res.getData()) {
      std::string res(client.res.getData());
      if (res.find("404 Not Found") != std::string::npos) {
        std::cout << GREEN << "[PASS] CGI 404 Not Found" << RESET << std::endl;
      } else {
        std::cout << RED << "[FAIL] CGI 404 | Status mismatch" << RESET
                  << std::endl;
      }
    } else {
      std::cout << RED << "[FAIL] CGI 404 | State mismatch or No Data"
                << " State: " << client.getState()
                << " Data: " << (client.res.getData() ? "OK" : "NULL") << RESET
                << std::endl;
    }
  }

  TestEnvironment::teardown();
  std::cout << "All tests finished." << std::endl;
  return 0;
}