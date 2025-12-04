#ifndef DEFINES_HPP
#define DEFINES_HPP

#include <iostream>
#include <string>

// サーバー全体の定数
#define MAX_URI_LENGTH 8192
#define MAX_HEADER_SIZE 16384
#define DEFAULT_CLIENT_MAX_BODY_SIZE 1048576  // 1MB (1024 * 1024)

// 多分これでいい
enum HttpMethod { GET, POST, DELETE, UNKNOWN_METHOD };

// クライアントの状態遷移（epollのイベント分岐に使用）
enum ConnState {
  WAIT_REQUEST,      // 接続直後 or Keep-Alive後のリクエスト待ち
  READING_REQUEST,   // 受信中 & パース中
  PROCESSING,        // 静的ファイルの準備など、すぐに終わる処理
  WAITING_CGI,       // CGIプロセスからの出力待ち（パイプ監視）
  WRITING_RESPONSE,  // レスポンス送信中
  KEEP_ALIVE,  // ※これはステートというより、WRITING完了後の「分岐フラグ」に近いかも
  CLOSE_CONNECTION  // 同上
};

// 適当に変えてもらって
enum ParseState {
  REQ_REQUEST_LINE,
  REQ_HEADERS,
  REQ_BODY,
  REQ_COMPLETE,
  REQ_ERROR
};

#endif
