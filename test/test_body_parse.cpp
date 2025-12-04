#include <cassert>
#include <cstring>
#include <iostream>
#include "../inc/Config.hpp"
#include "../inc/Http.hpp"

// 色付き出力用
#define GREEN "\033[32m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define RESET "\033[0m"

void printResult(const std::string& testName, bool result) {
  if (result) {
    std::cout << GREEN << "[PASS] " << testName << RESET << std::endl;
  } else {
    std::cout << RED << "[FAIL] " << testName << RESET << std::endl;
    exit(1);
  }
}

void printSection(const std::string& sectionName) {
  std::cout << YELLOW << "\n=== " << sectionName << " ===" << RESET
            << std::endl;
}

// =============================================================================
// テスト1: Content-Lengthヘッダーの値がメンバ変数に格納されるか
// =============================================================================
void test_ContentLength_Stored() {
  printSection("Content-Length Storage Test");

  HttpRequest req;
  const char* raw =
      "POST /upload HTTP/1.1\r\n"
      "Host: localhost:8080\r\n"
      "Content-Length: 13\r\n"
      "\r\n"
      "Hello, World!";

  bool completed = req.feed(raw, std::strlen(raw));

  printResult("ContentLength_Stored: Parse Complete", completed == true);
  printResult("ContentLength_Stored: Value check",
              req.getContentLength() == 13);

  std::vector<char> body = req.getBody();
  std::string bodyStr(body.begin(), body.end());
  printResult("ContentLength_Stored: Body Content", bodyStr == "Hello, World!");
}

// =============================================================================
// テスト2: Content-Length=0 の場合、ボディなしで完了
// =============================================================================
void test_ContentLength_Zero() {
  printSection("Content-Length Zero Test");

  HttpRequest req;
  const char* raw =
      "POST /empty HTTP/1.1\r\n"
      "Host: localhost:8080\r\n"
      "Content-Length: 0\r\n"
      "\r\n";

  bool completed = req.feed(raw, std::strlen(raw));

  printResult("ContentLength_Zero: Parse Complete", completed == true);
  printResult("ContentLength_Zero: Value check", req.getContentLength() == 0);
  printResult("ContentLength_Zero: Body Empty", req.getBody().empty());
}

// =============================================================================
// テスト3: ボディの分割受信 (Content-Length)
// =============================================================================
void test_Body_Fragmented() {
  printSection("Body Fragmented Reception Test");

  HttpRequest req;

  // 1回目: ヘッダー + ボディの一部
  const char* chunk1 =
      "POST /data HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Content-Length: 20\r\n"
      "\r\n"
      "12345";

  bool done1 = req.feed(chunk1, std::strlen(chunk1));
  printResult("Body_Fragmented: Chunk 1 Incomplete", done1 == false);
  printResult("Body_Fragmented: ContentLength set",
              req.getContentLength() == 20);

  // 2回目: ボディの続き
  const char* chunk2 = "67890";
  bool done2 = req.feed(chunk2, std::strlen(chunk2));
  printResult("Body_Fragmented: Chunk 2 Incomplete", done2 == false);

  // 3回目: 残りのボディ
  const char* chunk3 = "abcdefghij";
  bool done3 = req.feed(chunk3, std::strlen(chunk3));
  printResult("Body_Fragmented: Chunk 3 Complete", done3 == true);

  std::vector<char> body = req.getBody();
  std::string bodyStr(body.begin(), body.end());
  printResult("Body_Fragmented: Body Content",
              bodyStr == "1234567890abcdefghij");
}

// =============================================================================
// テスト4: ボディが指定より多く届いた場合（余剰データは次のリクエスト用）
// =============================================================================
void test_Body_ExcessData() {
  printSection("Body Excess Data Test");

  HttpRequest req;
  const char* raw =
      "POST /test HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Content-Length: 5\r\n"
      "\r\n"
      "HelloEXTRA";  // "Hello"(5バイト) + "EXTRA"(余分)

  bool completed = req.feed(raw, std::strlen(raw));

  printResult("Body_ExcessData: Parse Complete", completed == true);

  std::vector<char> body = req.getBody();
  std::string bodyStr(body.begin(), body.end());
  printResult("Body_ExcessData: Body is exactly 5 bytes", bodyStr == "Hello");
}

// =============================================================================
// テスト5: Content-Lengthが不正な形式 (数値以外)
// =============================================================================
void test_ContentLength_InvalidFormat() {
  printSection("Content-Length Invalid Format Test");

  HttpRequest req;
  const char* raw =
      "POST /bad HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Content-Length: abc\r\n"
      "\r\n";

  bool completed = req.feed(raw, std::strlen(raw));

  printResult("ContentLength_InvalidFormat: Not Complete", completed == false);
  printResult("ContentLength_InvalidFormat: Has Error", req.hasError() == true);
}

// =============================================================================
// テスト6: Content-Lengthがない場合、ボディなしで完了
// =============================================================================
void test_NoContentLength() {
  printSection("No Content-Length Test");

  HttpRequest req;
  const char* raw =
      "GET /page HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "\r\n";

  bool completed = req.feed(raw, std::strlen(raw));

  printResult("NoContentLength: Parse Complete", completed == true);
  printResult("NoContentLength: ContentLength is 0",
              req.getContentLength() == 0);
  printResult("NoContentLength: Body Empty", req.getBody().empty());
}

// =============================================================================
// テスト7: Content-Length と Transfer-Encoding の同時指定は禁止
// =============================================================================
void test_ConflictingHeaders() {
  printSection("Conflicting Headers Test");

  HttpRequest req;
  const char* raw =
      "POST /conflict HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Content-Length: 10\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n";

  bool completed = req.feed(raw, std::strlen(raw));

  printResult("ConflictingHeaders: Not Complete", completed == false);
  printResult("ConflictingHeaders: Has Error", req.hasError() == true);
}

// =============================================================================
// テスト8: バイナリデータ (NULL文字を含む)
// =============================================================================
void test_Body_BinaryData() {
  printSection("Body Binary Data Test");

  HttpRequest req;

  // ヘッダー部分
  const char* header =
      "POST /binary HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Content-Length: 10\r\n"
      "\r\n";

  // NULLを含むバイナリデータ
  char binaryBody[10] = {'A', 'B', '\0', 'C', 'D', '\0', 'E', 'F', 'G', 'H'};

  req.feed(header, std::strlen(header));
  bool completed = req.feed(binaryBody, 10);

  printResult("Body_BinaryData: Parse Complete", completed == true);

  std::vector<char> body = req.getBody();
  printResult("Body_BinaryData: Body Size", body.size() == 10);

  // NULL文字も含めて正しく保持されているか
  bool dataMatch =
      (body[0] == 'A' && body[1] == 'B' && body[2] == '\0' && body[3] == 'C' &&
       body[4] == 'D' && body[5] == '\0' && body[6] == 'E' && body[7] == 'F' &&
       body[8] == 'G' && body[9] == 'H');
  printResult("Body_BinaryData: Binary Content Preserved", dataMatch);
}

// =============================================================================
// テスト9: 大きなボディの分割受信
// =============================================================================
void test_Body_LargeFragmented() {
  printSection("Large Body Fragmented Test");

  HttpRequest req;
  const size_t bodySize = 1000;

  // ヘッダー送信
  std::ostringstream headerStream;
  headerStream << "POST /large HTTP/1.1\r\n"
               << "Host: localhost\r\n"
               << "Content-Length: " << bodySize << "\r\n"
               << "\r\n";
  std::string header = headerStream.str();

  bool done = req.feed(header.c_str(), header.size());
  printResult("Body_LargeFragmented: Header parsed", done == false);
  printResult("Body_LargeFragmented: ContentLength set",
              req.getContentLength() == bodySize);

  // 100バイトずつ10回に分けて送信
  for (size_t i = 0; i < 10; ++i) {
    char chunk[100];
    std::memset(chunk, 'A' + static_cast<char>(i), 100);
    done = req.feed(chunk, 100);

    if (i < 9) {
      printResult("Body_LargeFragmented: Chunk incomplete", done == false);
    } else {
      printResult("Body_LargeFragmented: Final chunk complete", done == true);
    }
  }

  printResult("Body_LargeFragmented: Body size correct",
              req.getBody().size() == bodySize);
}

// =============================================================================
// テスト10: Keep-Alive用のclear()後に再利用
// =============================================================================
void test_Clear_And_Reuse() {
  printSection("Clear and Reuse Test");

  HttpRequest req;

  // 1回目のリクエスト
  const char* raw1 =
      "POST /first HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Content-Length: 5\r\n"
      "\r\n"
      "Hello";

  bool done1 = req.feed(raw1, std::strlen(raw1));
  printResult("Clear_Reuse: First request complete", done1 == true);
  printResult("Clear_Reuse: First ContentLength", req.getContentLength() == 5);

  // リセット
  req.clear();
  printResult("Clear_Reuse: After clear ContentLength is 0",
              req.getContentLength() == 0);

  // 2回目のリクエスト
  const char* raw2 =
      "GET /second HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "\r\n";

  bool done2 = req.feed(raw2, std::strlen(raw2));
  printResult("Clear_Reuse: Second request complete", done2 == true);
  printResult("Clear_Reuse: Method changed to GET", req.getMethod() == GET);
  printResult("Clear_Reuse: Path changed", req.getPath() == "/second");
}

// =============================================================================
// main
// =============================================================================
int main() {
  std::cout << "=== Running Body Parse Tests ===" << std::endl;

  test_ContentLength_Stored();
  test_ContentLength_Zero();
  test_Body_Fragmented();
  test_Body_ExcessData();
  test_ContentLength_InvalidFormat();
  test_NoContentLength();
  test_ConflictingHeaders();
  test_Body_BinaryData();
  test_Body_LargeFragmented();
  test_Clear_And_Reuse();

  std::cout << GREEN << "\n=== All Body Parse Tests Passed! ===" << RESET
            << std::endl;
  return 0;
}
