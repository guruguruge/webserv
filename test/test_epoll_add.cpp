#include <sys/epoll.h>  // EPOLLIN
#include <unistd.h>     // pipe, close
#include <cstdio>       // perror
#include <cassert>
#include <cstdlib>
#include <iostream>
#include "../inc/EpollContext.hpp"
#include "../inc/EpollUtils.hpp"

// 色付け用
#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"

void printResult(const std::string& testName, bool success) {
  if (success) {
    std::cout << GREEN << "[PASS] " << testName << RESET << std::endl;
  } else {
    std::cout << RED << "[FAIL] " << testName << RESET << std::endl;
    std::exit(1);
  }
}

int main() {
  std::cout << "=== Starting EpollUtils::add Unit Test ===" << std::endl;

  try {
    // 1. EpollUtilsインスタンス作成
    EpollUtils epoll;
    std::cout << "EpollUtils created successfully." << std::endl;

    // テスト用のFDを用意 (pipeを使用)
    // pipe_fds[0]: 読み込み用, pipe_fds[1]: 書き込み用
    int pipe_fds[2];
    if (pipe(pipe_fds) < 0) {
      perror("pipe");
      return 1;
    }

    // Context作成 (ファクトリメソッドを使用)
    // Listener用コンテキストとしてダミーポート8080で作成
    EpollContext* ctx = EpollContext::createListener(8080);

    // ---------------------------------------------------------
    // TEST 1: 正常な追加
    // ---------------------------------------------------------
    {
      bool ret = epoll.add(pipe_fds[0], ctx, EPOLLIN);
      printResult("Add valid FD", ret == true);
    }

    // ---------------------------------------------------------
    // TEST 2: 重複登録のチェック
    // epollは同じFDを二重登録しようとするとエラー(EEXIST)になるはず
    // ---------------------------------------------------------
    {
      std::cout << "--- Expecting 'File exists' error below ---" << std::endl;
      bool ret = epoll.add(pipe_fds[0], ctx, EPOLLIN);
      printResult("Add duplicate FD (should fail)", ret == false);
    }

    // ---------------------------------------------------------
    // TEST 3: 無効なFDのチェック
    // ---------------------------------------------------------
    {
      int invalid_fd = -1;
      EpollContext* ctx2 = EpollContext::createListener(9090);
      std::cout << "--- Expecting 'Bad file descriptor' error below ---"
                << std::endl;
      bool ret = epoll.add(invalid_fd, ctx2, EPOLLIN);
      printResult("Add invalid FD (should fail)", ret == false);
      delete ctx2;
    }

    // ---------------------------------------------------------
    // TEST 4: 別のFDを追加 (CLIENT タイプ)
    // ---------------------------------------------------------
    {
      EpollContext* ctx3 = EpollContext::createClient(NULL);  // Client*なし
      bool ret = epoll.add(pipe_fds[1], ctx3, EPOLLOUT);
      printResult("Add second FD (write end)", ret == true);
      // 後始末
      epoll.del(pipe_fds[1]);
      delete ctx3;
    }

    // 後始末
    epoll.del(pipe_fds[0]);
    delete ctx;
    close(pipe_fds[0]);
    close(pipe_fds[1]);

  } catch (const std::exception& e) {
    std::cerr << RED << "[FATAL] Exception: " << e.what() << RESET << std::endl;
    return 1;
  }

  std::cout << "=== All Tests Passed ===" << std::endl;
  return 0;
}