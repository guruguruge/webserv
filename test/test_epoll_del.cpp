#include <sys/epoll.h>  // EPOLLIN
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
  std::cout << "=== Starting EpollUtils::del Unit Test ===" << std::endl;

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
    // TEST 1: 正常な削除
    // ---------------------------------------------------------
    {
      bool ret = epoll.del(pipe_fds[0]);
      printResult("Del: Remove registered FD", ret == true);
    }

    // ---------------------------------------------------------
    // TEST 2: 同じFDを再度削除 (既に削除済みなのでエラー)
    // ---------------------------------------------------------
    {
      std::cout << "--- Expecting 'No such file or directory' error below ---"
                << std::endl;
      bool ret = epoll.del(pipe_fds[0]);
      printResult("Del: Already removed FD (should fail)", ret == false);
    }

    // ---------------------------------------------------------
    // TEST 3: 未登録のFDを削除 (エラーになるはず)
    // ---------------------------------------------------------
    {
      std::cout << "--- Expecting 'No such file or directory' error below ---"
                << std::endl;
      bool ret = epoll.del(pipe_fds[1]);  // pipe_fds[1]は未登録
      printResult("Del: Unregistered FD (should fail)", ret == false);
    }

    // ---------------------------------------------------------
    // TEST 4: 無効なFDを削除 (エラーになるはず)
    // ---------------------------------------------------------
    {
      std::cout << "--- Expecting 'Bad file descriptor' error below ---"
                << std::endl;
      bool ret = epoll.del(-1);
      printResult("Del: Invalid FD (should fail)", ret == false);
    }

    // ---------------------------------------------------------
    // TEST 5: 削除後に再登録できるか確認
    // ---------------------------------------------------------
    {
      bool ret = epoll.add(pipe_fds[0], ctx, EPOLLIN);
      printResult("Re-add: Add FD after del", ret == true);

      ret = epoll.del(pipe_fds[0]);
      printResult("Del: Remove re-added FD", ret == true);
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
