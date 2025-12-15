#ifndef EPOLLCONTEXT_HPP
#define EPOLLCONTEXT_HPP

#include <cstddef>  // NULL

// 前方宣言 (循環参照回避)
class Client;

/*
 * EpollContext
 * 責務: epoll イベントのメタデータを保持
 *
 * main.cpp のイベントループで switch (ctx->type) により処理を分岐する
 */
struct EpollContext {
  // イベントの種類 (switch で分岐に使用)
  enum FdType {
    LISTENER,     // リスナーソケット (accept 用)
    CLIENT,       // クライアントソケット (read/write 用)
    CGI_STDOUT,   // CGI の標準出力パイプ (read 用)
    CGI_STDIN     // CGI の標準入力パイプ (write 用)
  };

  FdType type;

  // type によって使い分ける (union 的な使い方)
  Client* client;     // CLIENT, CGI_* の場合に有効
  int listen_port;    // LISTENER の場合に有効

  // --- ファクトリメソッド ---

  // Listener 用
  static EpollContext* createListener(int port) {
    EpollContext* ctx = new EpollContext();
    ctx->type = LISTENER;
    ctx->client = NULL;
    ctx->listen_port = port;
    return ctx;
  }

  // Client ソケット用
  static EpollContext* createClient(Client* c) {
    EpollContext* ctx = new EpollContext();
    ctx->type = CLIENT;
    ctx->client = c;
    ctx->listen_port = 0;
    return ctx;
  }

  // CGI パイプ用 (stdout/stdin)
  static EpollContext* createCgiPipe(Client* c, FdType pipeType) {
    EpollContext* ctx = new EpollContext();
    ctx->type = pipeType;
    ctx->client = c;
    ctx->listen_port = 0;
    return ctx;
  }

 private:
  // デフォルトコンストラクタは private (ファクトリメソッドを使用)
  EpollContext() : type(LISTENER), client(NULL), listen_port(0) {}
};

#endif