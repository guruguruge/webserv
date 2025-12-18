#include <sys/epoll.h>  // EPOLLIN, EPOLLOUT
#include <unistd.h>     // pipe, close, write
#include <cerrno>
#include <cstdio>   // perror
#include <cstdlib>
#include <cstring>  // strerror
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
  std::cout << "=== Starting EpollUtils::wait Unit Test ===" << std::endl;

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

    // 読み取り側を epoll に登録
    bool ret = epoll.add(pipe_fds[0], ctx, EPOLLIN);
    printResult("Setup: Add read FD with EPOLLIN", ret == true);

    // ---------------------------------------------------------
    // TEST 1: タイムアウト (イベントなし)
    // ---------------------------------------------------------
    {
      struct epoll_event events[10];
      int nfds = epoll.wait(events, 10, 100);  // 100msタイムアウト
      printResult("Wait: Timeout with no events", nfds == 0);
    }

    // ---------------------------------------------------------
    // TEST 2: パイプに書き込んでイベント発生
    // ---------------------------------------------------------
    {
      // パイプに書き込み
      const char* msg = "Hello";
      ssize_t written = write(pipe_fds[1], msg, 5);
      printResult("Setup: Write to pipe", written == 5);

      struct epoll_event events[10];
      int nfds = epoll.wait(events, 10, 100);
      printResult("Wait: Got event after write", nfds == 1);

      // イベントの内容を確認
      bool correctEvent = (events[0].events & EPOLLIN) != 0;
      printResult("Wait: Event is EPOLLIN", correctEvent);

      // Contextが正しく取れるか確認
      EpollContext* got_ctx = static_cast<EpollContext*>(events[0].data.ptr);
      printResult("Wait: Context matches", got_ctx == ctx);

      // パイプからデータを読み取り (バッファをクリア)
      char buf[16];
      read(pipe_fds[0], buf, sizeof(buf));
    }

    // ---------------------------------------------------------
    // TEST 3: 複数イベント
    // ---------------------------------------------------------
    {
      // 書き込み側も登録
      EpollContext* ctx2 = EpollContext::createClient(NULL);
      ret = epoll.add(pipe_fds[1], ctx2, EPOLLOUT);
      printResult("Setup: Add write FD with EPOLLOUT", ret == true);

      struct epoll_event events[10];
      int nfds = epoll.wait(events, 10, 100);
      // 書き込み側は常に書き込み可能なので、少なくとも1つはイベントがあるはず
      printResult("Wait: Got at least one event", nfds >= 1);

      // 後始末
      epoll.del(pipe_fds[1]);
      delete ctx2;
    }

    // ---------------------------------------------------------
    // TEST 4: max_events制限
    // ---------------------------------------------------------
    {
      // パイプに書き込み
      write(pipe_fds[1], "X", 1);

      struct epoll_event events[1];  // 1個だけ
      int nfds = epoll.wait(events, 1, 100);
      printResult("Wait: max_events=1 limits result", nfds <= 1);

      // パイプからデータを読み取り
      char buf[16];
      read(pipe_fds[0], buf, sizeof(buf));
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
