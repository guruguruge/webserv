#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../inc/Client.hpp"
#include "../inc/Config.hpp"
#include "../inc/EpollUtils.hpp"
#include "../inc/Http.hpp"
#include "../inc/RequestHandler.hpp"

// --- 色付き出力用マクロ ---
#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"
#define YELLOW "\033[33m"
#define CYAN "\033[36m"

// NOTE: src/EpollUtils.cpp をリンクするため、ここではモックを定義しません。
// テスト実行時に EpollUtils の実体が必要ですが、ClientにNULLを渡すため機能的には使いません。

// --- ヘルパー関数 ---
template <typename T>
std::string toString(const T& value) {
  std::ostringstream oss;
  oss << value;
  return oss.str();
}

// =============================================================================
// テスト環境セットアップ
// =============================================================================
class TestEnvironment {
 public:
  static std::string getAbsPath(const std::string& relativePath) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
      // パスの正規化などは省略するが、cwd + / + relativePath で構築
      std::string fullPath = std::string(cwd) + "/" + relativePath;
      return fullPath;
    }
    return relativePath;
  }

  static void setup() {
    // 念のためクリーンアップ
    teardown();

    std::string www = getAbsPath("test_www");
    std::string cgiBin = www + "/cgi-bin";
    std::string uploads = www + "/uploads";

    if (mkdir(www.c_str(), 0755) == -1 && errno != EEXIST) {
      std::cerr << RED << "mkdir failed: " << www << " : " << strerror(errno)
                << RESET << std::endl;
    }
    if (mkdir(cgiBin.c_str(), 0755) == -1 && errno != EEXIST) {
      std::cerr << RED << "mkdir failed: " << cgiBin << " : " << strerror(errno)
                << RESET << std::endl;
    }
    if (mkdir(uploads.c_str(), 0755) == -1 && errno != EEXIST) {
      std::cerr << RED << "mkdir failed: " << uploads << " : "
                << strerror(errno) << RESET << std::endl;
    }

    std::string pythonScript =
        "#!/usr/bin/python3\n"
        "import os, sys\n"
        "print('Status: 200 OK')\n"
        "print('Content-Type: text/plain')\n"
        "print('')\n"
        "print('Hello Python CGI')\n"
        "print(f'METHOD={os.environ.get(\"REQUEST_METHOD\")}')\n"
        "cl = os.environ.get('CONTENT_LENGTH')\n"
        "if cl:\n"
        "    try:\n"
        "        body = sys.stdin.read(int(cl))\n"
        "        print(f'BODY={body}')\n"
        "    except Exception as e:\n"
        "        print(f'BODY_READ_ERROR={e}')\n";

    std::string filePath = cgiBin + "/simple.py";
    createFile(filePath, pythonScript);
    chmod(filePath.c_str(), 0755);

    // 作成確認
    if (access(filePath.c_str(), R_OK | X_OK) != 0) {
      std::cerr << RED << "[FATAL] Created file is not accessible: " << filePath
                << RESET << std::endl;
    } else {
      std::cout << "[DEBUG] Script created at: " << filePath << std::endl;
    }
  }

  static void teardown() {
    std::string www = getAbsPath("test_www");
    // 簡易的な削除（エラー無視）
    unlink((www + "/cgi-bin/simple.py").c_str());
    rmdir((www + "/uploads").c_str());
    rmdir((www + "/cgi-bin").c_str());
    rmdir(www.c_str());
  }

  static void createFile(const std::string& path, const std::string& content) {
    std::ofstream ofs(path.c_str());
    if (!ofs) {
      std::cerr << RED << "[ERROR] Failed to create file: " << path << RESET
                << std::endl;
      return;
    }
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
  server.root = TestEnvironment::getAbsPath("test_www");

  LocationConfig locCgi;
  locCgi.path = "/cgi-bin/";
  locCgi.root = TestEnvironment::getAbsPath("test_www");
  locCgi.cgi_extension = ".py";
  locCgi.cgi_path = "/usr/bin/python3";
  locCgi.allow_methods.push_back(GET);
  locCgi.allow_methods.push_back(POST);

  server.locations.push_back(locCgi);
  config.servers.push_back(server);
}

// =============================================================================
// クライアントリクエストのセットアップ
// =============================================================================
void setupClientRequest(Client& client, const std::string& method,
                        const std::string& path, const std::string& body = "") {
  client.reset();
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

// =============================================================================
// イベントループシミュレーター
// =============================================================================
void simulateEventLoop(Client& client) {
  // 1. POSTデータ書き込み
  if (client.getState() == WAITING_CGI_INPUT) {
    int fd = client.getCgiStdinFd();
    if (fd != -1) {
      const std::vector<char>& body = client.req.getBody();
      if (!body.empty()) {
        write(fd, &body[0], body.size());
      }
      client.readyToCgiRead();
    }
  }

  // 2. CGI出力読み込み
  if (client.getState() == READING_CGI_OUTPUT) {
    int fd = client.getCgiStdoutFd();
    if (fd != -1) {
      // テスト用にブロッキングモードに戻す (EAGAINでループを抜けないようにするため)
      int flags = fcntl(fd, F_GETFL, 0);
      fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

      // 子プロセスの終了を待つ (パイプにデータが全て書き込まれるのを待つ)
      int status;
      waitpid(client.getCgiPid(), &status, 0);

      char buf[4096];
      ssize_t n;
      // EOF(0)になるまで読み切る
      while ((n = read(fd, buf, sizeof(buf))) > 0) {
        client.appendCgiOutput(buf, n);
      }

      // エラーチェック (EAGAIN以外で -1 ならエラー)
      if (n < 0) {
        std::cerr << RED << "[ERROR] read failed: " << strerror(errno) << RESET
                  << std::endl;
      }

      client.finishCgi();
    }
  }
}

// =============================================================================
// テスト実行
// =============================================================================
int main() {
  std::cout << CYAN << "=== Starting CGI Implementation Tests ===" << RESET
            << std::endl;

  TestEnvironment::setup();

  // ファイル存在確認
  std::string scriptPath =
      TestEnvironment::getAbsPath("test_www/cgi-bin/simple.py");
  std::cout << "[DEBUG] Checking access to: " << scriptPath << std::endl;
  if (access(scriptPath.c_str(), R_OK) != 0) {
    std::cerr << RED << "[FATAL] Cannot read script: " << strerror(errno)
              << RESET << std::endl;
    TestEnvironment::teardown();
    return 1;
  }

  MainConfig config;
  setupTestConfig(config);
  RequestHandler handler(config);
  Client client(999, 8080, "127.0.0.1", NULL);

  // ---------------------------------------------------------
  // TEST 1: CGI GET Request
  // ---------------------------------------------------------
  {
    std::cout << "\n"
              << YELLOW << "[TEST 1] CGI GET Request" << RESET << std::endl;
    setupClientRequest(client, "GET", "/cgi-bin/simple.py");

    handler.handle(&client);

    if (client.getState() == READING_CGI_OUTPUT) {
      std::cout << "State transition: " << GREEN << "OK" << RESET
                << " (READING_CGI_OUTPUT)" << std::endl;
      simulateEventLoop(client);

      if (client.getState() == WRITING_RESPONSE) {
        client.res.build();
        const char* data = client.res.getData();
        size_t size = client.res.getRemainingSize();

        if (data && size > 0) {
          std::string responseStr(data, size);
          if (responseStr.find("Hello Python CGI") != std::string::npos) {
            std::cout << GREEN << "[PASS] Response Body Correct" << RESET
                      << std::endl;
          } else {
            std::cout << RED << "[FAIL] Unexpected Body:\n"
                      << responseStr << RESET << std::endl;
          }
        } else {
          std::cout << RED << "[FAIL] No Response Data" << RESET << std::endl;
        }
      } else {
        std::cout << RED << "[FAIL] Final State is not WRITING_RESPONSE"
                  << RESET << std::endl;
      }
    } else {
      std::cout << RED
                << "[FAIL] CGI did not start. State: " << client.getState()
                << RESET << std::endl;
    }
  }

  // ---------------------------------------------------------
  // TEST 2: CGI POST Request
  // ---------------------------------------------------------
  {
    std::cout << "\n"
              << YELLOW << "[TEST 2] CGI POST Request" << RESET << std::endl;
    std::string postData = "MyPostData123";
    setupClientRequest(client, "POST", "/cgi-bin/simple.py", postData);

    handler.handle(&client);

    if (client.getState() == WAITING_CGI_INPUT) {
      std::cout << "State transition: " << GREEN << "OK" << RESET
                << " (WAITING_CGI_INPUT)" << std::endl;
      simulateEventLoop(client);

      if (client.getState() == WRITING_RESPONSE) {
        client.res.build();
        const char* data = client.res.getData();
        size_t size = client.res.getRemainingSize();

        if (data && size > 0) {
          std::string responseStr(data, size);
          if (responseStr.find("BODY=" + postData) != std::string::npos) {
            std::cout << GREEN << "[PASS] POST Data Echoed Correctly" << RESET
                      << std::endl;
          } else {
            std::cout << RED << "[FAIL] Unexpected Body:\n"
                      << responseStr << RESET << std::endl;
          }
        } else {
          std::cout << RED << "[FAIL] No Response Data" << RESET << std::endl;
        }
      } else {
        std::cout << RED << "[FAIL] Final State is not WRITING_RESPONSE"
                  << RESET << std::endl;
      }
    } else {
      std::cout << RED << "[FAIL] CGI did not start correctly for POST. State: "
                << client.getState() << RESET << std::endl;
    }
  }

  // ---------------------------------------------------------
  // TEST 3: CGI 404 (Script Not Found)
  // ---------------------------------------------------------
  {
    std::cout << "\n"
              << YELLOW << "[TEST 3] CGI 404 Not Found" << RESET << std::endl;
    setupClientRequest(client, "GET", "/cgi-bin/ghost.py");
    handler.handle(&client);

    if (client.getState() == WRITING_RESPONSE) {
      client.res.build();
      const char* rawData = client.res.getData();
      if (rawData && std::string(rawData).find("404") != std::string::npos) {
        std::cout << GREEN << "[PASS] 404 Error Generated" << RESET
                  << std::endl;
      } else {
        std::cout << RED << "[FAIL] Status Code mismatch" << RESET << std::endl;
      }
    } else {
      std::cout << RED << "[FAIL] Invalid State: " << client.getState() << RESET
                << std::endl;
    }
  }

  TestEnvironment::teardown();
  std::cout << "\n"
            << CYAN << "=== All tests finished ===" << RESET << std::endl;
  return 0;
}