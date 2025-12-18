#include <sys/epoll.h>  // EPOLLIN, EPOLLOUT
#include <unistd.h>     // pipe, close
#include <cstdio>       // perror
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
  std::cout << "=== Starting EpollUtils::mod Unit Test ===" << std::endl;

  try {
    EpollUtils epoll;
    std::cout << "EpollUtils created successfully." << std::endl;

    // テスト用のFDを用意 (pipeを使用)
    int pipe_fds[2];
    if (pipe(pipe_fds) < 0) {
      perror("pipe");
      return 1;
    }

    // Context作成
    EpollContext* ctx = EpollContext::createListener(8080);

    // ---------------------------------------------------------
    // 準備: まず add で登録
    // ---------------------------------------------------------
    {
      bool ret = epoll.add(pipe_fds[0], ctx, EPOLLIN);
      printResult("Setup: Add FD with EPOLLIN", ret == true);
    }

    // ---------------------------------------------------------
    // TEST 1: EPOLLIN -> EPOLLOUT に変更
    // ---------------------------------------------------------
    {
      bool ret = epoll.mod(pipe_fds[0], ctx, EPOLLOUT);
      printResult("Mod: EPOLLIN -> EPOLLOUT", ret == true);
    }

    // ---------------------------------------------------------
    // TEST 2: EPOLLOUT -> EPOLLIN に戻す
    // ---------------------------------------------------------
    {
      bool ret = epoll.mod(pipe_fds[0], ctx, EPOLLIN);
      printResult("Mod: EPOLLOUT -> EPOLLIN", ret == true);
    }

    // ---------------------------------------------------------
    // TEST 3: 複合イベント EPOLLIN | EPOLLOUT
    // ---------------------------------------------------------
    {
      bool ret = epoll.mod(pipe_fds[0], ctx, EPOLLIN | EPOLLOUT);
      printResult("Mod: EPOLLIN | EPOLLOUT", ret == true);
    }

    // ---------------------------------------------------------
    // TEST 4: 未登録のFDを mod しようとする (エラーになるはず)
    // ---------------------------------------------------------
    {
      EpollContext* ctx2 = EpollContext::createListener(9090);
      std::cout << "--- Expecting 'No such file or directory' error below ---"
                << std::endl;
      bool ret = epoll.mod(pipe_fds[1], ctx2, EPOLLIN);  // pipe_fds[1]は未登録
      printResult("Mod: Unregistered FD (should fail)", ret == false);
      delete ctx2;
    }

    // ---------------------------------------------------------
    // TEST 5: 無効なFDを mod しようとする (エラーになるはず)
    // ---------------------------------------------------------
    {
      std::cout << "--- Expecting 'Bad file descriptor' error below ---"
                << std::endl;
      bool ret = epoll.mod(-1, ctx, EPOLLIN);
      printResult("Mod: Invalid FD (should fail)", ret == false);
    }

    // 後始末
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
