#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "Http.hpp"

// 色付き出力用
#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"

// -----------------------------------------------------------------------------
// ヘルパー関数
// -----------------------------------------------------------------------------

// バッファを文字列として取得（デバッグ用）
std::string getRawBuffer(const HttpResponse& res) {
  return std::string(res._responseBuffer.begin(), res._responseBuffer.end());
}

// チャンクレスポンスの妥当性をチェックする関数
void inspectChunkedResponse(const HttpResponse& res,
                            const std::string& testName,
                            size_t originalBodySize) {
  std::cout << "Checking [" << testName << "] ... ";
  bool ok = true;

  // friend関数経由でデータ取得
  std::string raw = getRawBuffer(res);

  // 1. ヘッダーチェック
  if (raw.find("Transfer-Encoding: chunked") == std::string::npos) {
    std::cout << "\n  [NG] Missing 'Transfer-Encoding: chunked'";
    ok = false;
  }
  if (raw.find("Content-Length:") != std::string::npos) {
    std::cout << "\n  [NG] Found forbidden 'Content-Length' header";
    ok = false;
  }

  // 2. 終端チャンクチェック (0\r\n\r\n)
  if (raw.find("\r\n0\r\n\r\n") == std::string::npos) {
    if (raw.find("0\r\n\r\n") == std::string::npos) {
      std::cout << "\n  [NG] Missing end chunk (0\\r\\n\\r\\n)";
      ok = false;
    }
  }

  // 3. データサイズの簡易検証
  if (res._responseBuffer.size() <= originalBodySize) {
    std::cout << "\n  [NG] Buffer size too small (Chunks missing?)";
    ok = false;
  }

  if (ok)
    std::cout << GREEN << "OK" << RESET << std::endl;
  else
    std::cout << RED << "NG" << RESET << std::endl;
}

int main() {
  std::cout << "=== PR6: Chunked Transfer Encoding Test ===" << std::endl;

  // -------------------------------------------------------------------------
  // TEST 1: 小さなデータのチャンク化 (Small Data)
  // -------------------------------------------------------------------------
  {
    HttpResponse res;
    res.setBody("HelloWorld");  // 10バイト
    res.setChunked(true);
    res.build();

    inspectChunkedResponse(res, "Small Data (10 bytes)", 10);

    // 具体的な中身のチェック
    std::string raw = getRawBuffer(res);

    // サイズ "a" (10の16進数) があるか
    if (raw.find("\r\na\r\nHelloWorld\r\n") != std::string::npos) {
      std::cout << "  -> Format check: " << GREEN << "OK" << RESET << std::endl;
    } else {
      std::cout << "  -> Format check: " << RED << "NG" << RESET
                << " (Expected 'a\\r\\nHelloWorld\\r\\n')" << std::endl;
    }
  }

  // -------------------------------------------------------------------------
  // TEST 2: 大きなデータの分割 (Large Data > 1024)
  // -------------------------------------------------------------------------
  {
    // 2500バイトのデータを作成 (1024 + 1024 + 452 に分割されるはず)
    std::string largeData(2500, 'A');

    HttpResponse res;
    res.setBody(largeData);
    res.setChunked(true);
    res.build();

    inspectChunkedResponse(res, "Large Data (2500 bytes)", 2500);

    std::string raw = getRawBuffer(res);

    // 1つ目のチャンクサイズ "400" (1024の16進数) を探す
    // 3つ目のチャンクサイズ "1c4" (452の16進数) を探す

    bool chunk1 = (raw.find("400\r\n") != std::string::npos);
    bool chunk3 = (raw.find("1c4\r\n") != std::string::npos);  // 452 = 0x1C4

    if (chunk1 && chunk3) {
      std::cout << "  -> Split logic: " << GREEN << "OK" << RESET
                << " (Found 0x400 and 0x1c4 chunks)" << std::endl;
    } else {
      std::cout << "  -> Split logic: " << RED << "NG" << RESET << std::endl;
    }
  }

  // -------------------------------------------------------------------------
  // TEST 3: 空ボディのチャンク化 (Empty Body)
  // -------------------------------------------------------------------------
  {
    HttpResponse res;
    res.setBody("");
    res.setChunked(true);
    res.build();

    inspectChunkedResponse(res, "Empty Body", 0);

    std::string raw = getRawBuffer(res);
    // ヘッダー直後に 0\r\n\r\n が来ているか
    if (raw.find("\r\n\r\n0\r\n\r\n") != std::string::npos) {
      std::cout << "  -> Empty format: " << GREEN << "OK" << RESET << std::endl;
    } else {
      std::cout << "  -> Empty format: " << RED << "NG" << RESET << std::endl;
    }
  }

  // -------------------------------------------------------------------------
  // TEST 4: 禁止ステータスでのチャンク無効化 (204 No Content)
  // -------------------------------------------------------------------------
  {
    std::cout << "Checking [204 No Content] ... ";
    HttpResponse res;
    res.setStatusCode(204);
    res.setBody("Should Not Be Sent");
    res.setChunked(true);  // チャンクONにするが...
    res.build();

    std::string raw = getRawBuffer(res);

    // 期待値: ボディなし、Content-Lengthなし、Transfer-Encodingなし
    bool ok = true;
    if (raw.find("Transfer-Encoding") != std::string::npos)
      ok = false;
    if (raw.find("Content-Length") != std::string::npos)
      ok = false;
    if (raw.find("Should Not Be Sent") != std::string::npos)
      ok = false;

    if (ok)
      std::cout << GREEN << "OK" << RESET << std::endl;
    else
      std::cout << RED << "NG (Forbidden headers or body found)" << RESET
                << std::endl;
  }

  return 0;
}
