#include <iostream>
#include <string>
#include "Http.hpp"

// 色付き出力用
#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"

// friend関数を使った検証ヘルパー
void inspectErrorResponse(const HttpResponse& res, int expCode,
                          const std::string& expMsg) {
  std::cout << "Checking Error Response (" << expCode << ")... ";

  bool ok = true;

  // 1. ステータスコード
  if (res._statusCode != expCode) {
    std::cout << "\n  [NG] Code: " << res._statusCode
              << " (Expected: " << expCode << ")";
    ok = false;
  }

  // 2. ステータスメッセージ
  if (res._statusMessage != expMsg) {
    std::cout << "\n  [NG] Msg: " << res._statusMessage
              << " (Expected: " << expMsg << ")";
    ok = false;
  }

  // 3. Content-Type
  if (res._headers.count("Content-Type") == 0 ||
      res._headers.at("Content-Type") != "text/html") {
    std::cout << "\n  [NG] Content-Type is incorrect";
    ok = false;
  }

  // 4. Bodyの中身（HTMLになっているか簡易チェック）
  std::string bodyStr(res._body.begin(), res._body.end());
  if (bodyStr.find("<html>") == std::string::npos ||
      bodyStr.find(expMsg) == std::string::npos) {
    std::cout << "\n  [NG] Body does not look like error HTML";
    ok = false;
  }

  if (ok)
    std::cout << GREEN << "OK" << RESET << std::endl;
  else
    std::cout << RED << "NG" << RESET << std::endl;
}

void inspectClear(const HttpResponse& res) {
  std::cout << "Checking clear()... ";
  bool ok = true;

  if (res._statusCode != 200)
    ok = false;
  if (res._statusMessage != "OK")
    ok = false;
  if (!res._headers.empty())
    ok = false;
  if (!res._body.empty())
    ok = false;
  if (!res._responseBuffer.empty())
    ok = false;
  if (res._sentBytes != 0)
    ok = false;

  if (ok)
    std::cout << GREEN << "OK" << RESET << std::endl;
  else
    std::cout << RED << "NG (Some fields are not reset)" << RESET << std::endl;
}

// 追加: 再利用時のチェック用ヘルパー
void inspectReuse(const HttpResponse& res, int expCode) {
  if (res._statusCode == expCode) {
    std::cout << "Reuse after clear: " << GREEN << "OK" << RESET << std::endl;
  } else {
    std::cout << "Reuse after clear: " << RED << "NG (Expected " << expCode
              << ", got " << res._statusCode << ")" << RESET << std::endl;
  }
}

int main() {
  std::cout << "=== PR5: Error Response & Clear Test ===" << std::endl;

  HttpResponse res;

  // ---------------------------------------------------------
  // TEST 1: 404 Not Found 生成
  // ---------------------------------------------------------
  res.makeErrorResponse(404);
  inspectErrorResponse(res, 404, "Not Found");

  // ---------------------------------------------------------
  // TEST 2: 500 Internal Server Error 生成
  // ---------------------------------------------------------
  // 上書きして大丈夫か確認
  res.makeErrorResponse(500);
  inspectErrorResponse(res, 500, "Internal Server Error");

  // ---------------------------------------------------------
  // TEST 3: Clear 機能
  // ---------------------------------------------------------
  // データを汚してから clear
  res.makeErrorResponse(403);
  res.build();  // バッファも作る

  res.clear();
  inspectClear(res);

  // clear後に普通に使えるか
  res.setStatusCode(200);
  inspectReuse(res, 200);

  return 0;
}
