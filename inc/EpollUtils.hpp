#ifndef EPOLLUTILS_HPP
#define EPOLLUTILS_HPP

#include <stdint.h>
#include <sys/epoll.h>
#include "EpollContext.hpp"

class EpollUtils {
 private:
  int _epoll_fd;

 public:
  EpollUtils();
  ~EpollUtils();

  // 監視追加 (ADD)
  // Contextのポインタを受け取ることで、呼び出し側でのキャスト忘れを防ぐ
  bool add(int fd, EpollContext* ctx, uint32_t events);

  // 監視変更 (MOD)
  // 読み書きの切り替え用
  bool mod(int fd, EpollContext* ctx, uint32_t events);

  // 監視削除 (DEL)
  bool del(int fd);

  // 待機 (WAIT)
  // ラッパー関数にすることでエラー処理を共通化
  int wait(struct epoll_event* events, int max_events, int timeout_ms);
};

#endif