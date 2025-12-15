#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "Http.hpp"

// 色付き出力用
#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"

// ダミーファイル作成
void createDummyFile(const std::string& filename, size_t size) {
  std::ofstream ofs(filename.c_str(), std::ios::binary);
  // 'A' から始まるパターンデータを書き込む
  for (size_t i = 0; i < size; ++i) {
    char c = 'A' + (i % 26);
    ofs.write(&c, 1);
  }
  ofs.close();
}

// サーバーの送信ループをシミュレートする関数
// 戻り値: 実際にクライアントに送られた全データ
std::string simulateStreaming(HttpResponse& res) {
  std::string totalOutput;
  int loopCount = 0;

  // 1. 初回の準備
  res.build();

  // 2. 送信ループ (isDoneになるまで繰り返す)
  while (!res.isDone()) {
    const char* data = res.getData();
    size_t len = res.getRemainingSize();

    // データがあれば送信バッファに追加（送信したつもり）
    if (data && len > 0) {
      totalOutput.append(data, len);
      res.advance(len);  // 全て送れたとする
    } else {
      // データがない場合（バッファが空）、
      // advance(0) を呼んで次のデータを補充させる
      res.advance(0);
    }

    loopCount++;
    // 無限ループ防止（安全策）
    if (loopCount > 10000) {
      std::cerr << RED << "[FATAL] Infinite loop detected!" << RESET
                << std::endl;
      break;
    }
  }
  return totalOutput;
}

int main() {
  std::cout << "=== PR7: Streaming & State Machine Test ===" << std::endl;

  const std::string kFileName = "test_large.bin";
  const size_t kFileSize = 5000;  // チャンクサイズ(1024)の約5倍

  // テストファイル作成
  createDummyFile(kFileName, kFileSize);

  // -------------------------------------------------------------------------
  // TEST 1: 通常モード (Content-Length) でのストリーミング
  // -------------------------------------------------------------------------
  {
    std::cout << "\n[TEST 1] Normal Streaming (Content-Length)" << std::endl;
    HttpResponse res;

    if (!res.setBodyFile(kFileName)) {
      std::cerr << RED << "Failed to open file" << RESET << std::endl;
      return 1;
    }
    res.setChunked(false);

    // シミュレーション実行
    std::string output = simulateStreaming(res);

    // 検証
    bool ok = true;

    // ヘッダー確認
    if (output.find("Content-Length: 5000") == std::string::npos) {
      std::cout << "  [NG] Content-Length header missing or wrong" << std::endl;
      ok = false;
    }

    // ボディの抽出（\r\n\r\n の後）
    size_t bodyPos = output.find("\r\n\r\n");
    if (bodyPos == std::string::npos) {
      std::cout << "  [NG] Header/Body separator not found" << std::endl;
      ok = false;
    } else {
      std::string body = output.substr(bodyPos + 4);
      if (body.size() != kFileSize) {
        std::cout << "  [NG] Body size mismatch. Expected " << kFileSize
                  << ", got " << body.size() << std::endl;
        ok = false;
      } else {
        // 内容チェック（先頭と末尾）
        if (body[0] != 'A' ||
            body[kFileSize - 1] != 'A' + ((kFileSize - 1) % 26)) {
          std::cout << "  [NG] Data corruption detected" << std::endl;
          ok = false;
        }
      }
    }

    if (ok)
      std::cout << GREEN << "OK" << RESET << std::endl;
    else
      std::cout << RED << "NG" << RESET << std::endl;
  }

  // -------------------------------------------------------------------------
  // TEST 2: チャンクモード (Chunked) でのストリーミング
  // -------------------------------------------------------------------------
  {
    std::cout << "\n[TEST 2] Chunked Streaming" << std::endl;
    HttpResponse res;

    res.setBodyFile(kFileName);
    res.setChunked(true);

    // シミュレーション実行
    std::string output = simulateStreaming(res);

    // 検証
    bool ok = true;

    // ヘッダー確認
    if (output.find("Transfer-Encoding: chunked") == std::string::npos) {
      std::cout << "  [NG] Transfer-Encoding header missing" << std::endl;
      ok = false;
    }

    // チャンク構造の確認
    // 1024バイトごとのチャンク (サイズ 400) が複数あるはず
    if (output.find("\r\n400\r\n") == std::string::npos) {
      std::cout << "  [NG] Chunk size 400 (1024) not found" << std::endl;
      ok = false;
    }

    // 終端チャンク確認
    if (output.find("\r\n0\r\n\r\n") == std::string::npos) {
      std::cout << "  [NG] End chunk (0\\r\\n\\r\\n) not found" << std::endl;
      ok = false;
    }

    if (ok)
      std::cout << GREEN << "OK" << RESET << std::endl;
    else
      std::cout << RED << "NG" << RESET << std::endl;
  }

  // 後始末
  std::remove(kFileName.c_str());

  return 0;
}