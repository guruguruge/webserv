#include <cstring>
#include <iostream>
#include <vector>
#include "../inc/Http.hpp"

#define GREEN "\033[32m"
#define RESET "\033[0m"

void test_BuildResponse() {
  std::cout << "--- Testing Build Response ---" << std::endl;
  HttpResponse res;

  res.setStatusCode(200);
  res.setHeader("Server", "Webserv/1.0");
  res.setHeader("Content-Type", "text/html");
  res.setBody("<h1>Hello World</h1>");

  res.build();

  // 生データ取得
  std::string raw(res.getData(), res.getRemainingSize());

  std::cout << "Generated Response:\n"
            << "--------------------------------\n"
            << raw << "\n--------------------------------" << std::endl;

  // 簡易チェック
  if (raw.find("HTTP/1.1 200 OK") != std::string::npos &&
      raw.find("Content-Length: 20") != std::string::npos &&
      raw.find("<h1>Hello World</h1>") != std::string::npos) {
    std::cout << GREEN << "[PASS] Response format looks correct." << RESET
              << std::endl;
  } else {
    std::cerr << "[FAIL] Response format is wrong." << std::endl;
  }
}

// 送信ループのシミュレーション (epollで少しずつ送る挙動)
void test_SendingLoop() {
  std::cout << "\n--- Testing Sending Loop (advance) ---" << std::endl;
  HttpResponse res;
  res.setStatusCode(404);
  res.setBody("Not Found");
  res.build();

  size_t totalSize = res.getRemainingSize();
  size_t sentTotal = 0;

  // 3バイトずつ送信するシミュレーション
  while (!res.isDone()) {
    size_t remain = res.getRemainingSize();
    size_t sendChunk = (remain > 3) ? 3 : remain;

    // getData() のポインタがちゃんと進んでいるか確認
    // 実際にはここで send() システムコールを呼ぶ
    std::cout << "Sending " << sendChunk << " bytes... (Remaining: " << remain
              << ")" << std::endl;

    res.advance(sendChunk);
    sentTotal += sendChunk;
  }

  if (sentTotal == totalSize) {
    std::cout << GREEN << "[PASS] All bytes sent correctly." << RESET
              << std::endl;
  } else {
    std::cerr << "[FAIL] Size mismatch." << std::endl;
  }
}

int main() {
  test_BuildResponse();
  test_SendingLoop();
  return 0;
}

// 2. test_HttpResponse.cpp 用 (送信・構築) ttana
// A. 正常系: レスポンス構築
//     test_Build_Simple200OK(): ステータス、ContentType、Bodyをセットして正しい文字列になるか。
//     test_Build_Redirect302(): Location ヘッダーを含むリダイレクトレスポンスが作れるか。
//     test_Build_404NotFound(): エラーページ用のHTMLをBodyにセットできるか。
//     test_Build_BinaryBody(): 画像ファイルの中身（バイナリ）をセットしても壊れないか。
//     test_Auto_ContentLength(): Bodyをセットしただけで、自動的に正しい Content-Length ヘッダーが付与されるか。
// B. 正常系: 送信バッファ管理 (epoll対応)
//     test_Advance_FullSend(): response.getData() の全サイズを一気に advance() した場合、isDone() が true になるか。
//     test_Advance_StepByStep(): 100バイトのレスポンスを、1バイトずつ100回 advance() しても、ポインタと残りサイズ (getRemainingSize) が正しく推移するか。
//     test_Advance_OverAdvance(): 万が一残りサイズより多く advance() された時の安全性（assertするか無視するか）。