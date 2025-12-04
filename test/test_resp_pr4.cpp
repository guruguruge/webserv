#include <cassert>
#include <fstream>
#include <iostream>
#include <vector>
#include "Http.hpp"

// 色付き出力用
#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"

// テスト用のダミーファイルを作成するヘルパー関数
void createDummyFile(const std::string& filename, const char* data,
                     size_t size) {
  std::ofstream ofs(filename.c_str(), std::ios::out | std::ios::binary);
  ofs.write(data, size);
  ofs.close();
}

// 判定用関数 (基本版: friendとして実装)
void inspectResponse(const HttpResponse& res, const std::string& checkName,
                     size_t expSize, const std::string& expType) {
  std::cout << "Checking [" << checkName << "] ... ";

  // 1. ボディサイズの確認
  if (res._body.size() != expSize) {
    std::cout << RED << "NG" << RESET << std::endl;
    std::cout << "  - Body Size: Expected " << expSize << ", got "
              << res._body.size() << std::endl;
    return;
  }

  // 2. Content-Typeの確認
  std::map<std::string, std::string>::const_iterator it =
      res._headers.find("Content-Type");
  std::string actualType = (it != res._headers.end()) ? it->second : "(none)";

  if (actualType != expType) {
    std::cout << RED << "NG" << RESET << std::endl;
    std::cout << "  - Content-Type: Expected '" << expType << "', got '"
              << actualType << "'" << std::endl;
    return;
  }

  std::cout << GREEN << "OK" << RESET << std::endl;
}

// 判定用関数 (バイナリ比較版オーバーロード: friendとして実装)
// 追加: expDataと比較を行う
void inspectResponse(const HttpResponse& res, const std::string& checkName,
                     const char* expData, size_t expSize,
                     const std::string& expType) {
  std::cout << "Checking [" << checkName << "] ... ";

  // 1. ボディサイズの確認
  if (res._body.size() != expSize) {
    std::cout << RED << "NG" << RESET << std::endl;
    std::cout << "  - Body Size: Expected " << expSize << ", got "
              << res._body.size() << std::endl;
    return;
  }

  // 2. バイナリデータの比較 (friendなのでアクセス可能)
  bool binaryMatch = true;
  for (size_t i = 0; i < expSize; ++i) {
    if (res._body[i] != expData[i]) {
      binaryMatch = false;
      break;
    }
  }

  if (!binaryMatch) {
    std::cout << RED << "NG" << RESET << std::endl;
    std::cout << "  - Binary Data: Mismatch detected!" << std::endl;
    return;
  }

  // 3. Content-Typeの確認
  std::map<std::string, std::string>::const_iterator it =
      res._headers.find("Content-Type");
  std::string actualType = (it != res._headers.end()) ? it->second : "(none)";

  if (actualType != expType) {
    std::cout << RED << "NG" << RESET << std::endl;
    std::cout << "  - Content-Type: Expected '" << expType << "', got '"
              << actualType << "'" << std::endl;
    return;
  }

  std::cout << GREEN << "OK" << RESET << std::endl;
}

int main() {
  std::cout << "=== PR4: File Loading & MIME Type Test ===" << std::endl;

  // -------------------------------------------------------------------------
  // TEST 1: テキストファイルの読み込み
  // -------------------------------------------------------------------------
  {
    std::string filename = "test_text.txt";
    std::string content = "Hello World\nThis is a text file.";
    createDummyFile(filename, content.c_str(), content.size());

    HttpResponse res;
    bool result = res.setBodyFile(filename);

    if (result) {
      inspectResponse(res, "Text File", content.size(), "text/plain");
    } else {
      std::cout << RED << "[NG] Text File: Failed to open" << RESET
                << std::endl;
    }
    std::remove(filename.c_str());  // お掃除
  }

  // -------------------------------------------------------------------------
  // TEST 2: バイナリファイル（画像）の読み込み
  // ヌル文字(\0)を含めて正しく読み込めるか、拡張子.pngを認識するか
  // -------------------------------------------------------------------------
  {
    std::string filename = "test_image.png";
    // 擬似的なバイナリデータ (ヌル文字を含む)
    char rawData[] = {(char)0x89, 'P', 'N', 'G', 0x00, 0x01, 0x02, (char)0xFF};
    createDummyFile(filename, rawData, sizeof(rawData));

    HttpResponse res;
    bool result = res.setBodyFile(filename);

    if (result) {
      // 修正: friend関数内で比較を行うように変更
      inspectResponse(res, "Binary(PNG) File", rawData, sizeof(rawData),
                      "image/png");
    } else {
      std::cout << RED << "[NG] Binary File: Failed to open" << RESET
                << std::endl;
    }
    std::remove(filename.c_str());
  }

  // -------------------------------------------------------------------------
  // TEST 3: 存在しないファイルの読み込み
  // -------------------------------------------------------------------------
  {
    std::cout << "Checking [Non-existent File] ... ";
    HttpResponse res;
    bool result = res.setBodyFile("ghost_file.txt");

    if (result == false) {
      std::cout << GREEN << "OK (Correctly returned false)" << RESET
                << std::endl;
    } else {
      std::cout << RED << "NG (Returned true but file shouldn't exist)" << RESET
                << std::endl;
    }
  }

  // -------------------------------------------------------------------------
  // TEST 4: 空ファイルの読み込み
  // -------------------------------------------------------------------------
  {
    std::string filename = "empty.html";
    createDummyFile(filename, "", 0);

    HttpResponse res;
    bool result = res.setBodyFile(filename);

    if (result) {
      inspectResponse(res, "Empty File", 0, "text/html");
    } else {
      std::cout << RED << "[NG] Empty File: Failed to open" << RESET
                << std::endl;
    }
    std::remove(filename.c_str());
  }

  return 0;
}