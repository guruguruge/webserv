#ifndef DEFINES_HPP
#define DEFINES_HPP

#include <string>
#include <iostream>

// サーバー全体の定数
#define MAX_URI_LENGTH 8192
#define MAX_HEADER_SIZE 16384

// 多分これでいい
enum HttpMethod
{
    GET,
    POST,
    DELETE,
    UNKNOWN_METHOD
};

// クライアントの状態遷移（epollのイベント分岐に使用）
enum ConnState
{
    WAIT_REQUEST,     // リクエスト受信待ち
    READING_REQUEST,  // 受信中 & パース中
    PROCESSING,       // パース完了 -> 応答生成中（CGI待機やファイル読込）
    WRITING_RESPONSE, // レスポンス送信中
    KEEP_ALIVE,       // 送信完了 -> 次のリクエスト待ちへリセット
    CLOSE_CONNECTION  // 送信完了 -> 切断
};

// 適当に変えてもらって
enum ParseState
{
    REQ_REQUEST_LINE,
    REQ_HEADERS,
    REQ_BODY,
    REQ_COMPLETE,
    REQ_ERROR
};

#endif
