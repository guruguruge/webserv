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
// テスト11: Content-Length が client_max_body_size を超過した場合
// =============================================================================
void test_BodyTooLarge() {
  printSection("Body Too Large Test");

  // ServerConfigを設定（client_max_body_size = 100）
  ServerConfig config;
  config.listen_port = 8080;
  config.host = "127.0.0.1";
  config.client_max_body_size = 100;

  HttpRequest req;
  req.setConfig(&config);

  // Content-Length: 200 (制限の100を超過)
  const char* raw =
      "POST /upload HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Content-Length: 200\r\n"
      "\r\n";

  bool completed = req.feed(raw, std::strlen(raw));

  printResult("BodyTooLarge: Not Complete", completed == false);
  printResult("BodyTooLarge: Has Error", req.hasError() == true);
}

// =============================================================================
// テスト12: Content-Length がちょうど client_max_body_size と同じ場合 (OK)
// =============================================================================
void test_BodyExactLimit() {
  printSection("Body Exact Limit Test");

  ServerConfig config;
  config.listen_port = 8080;
  config.host = "127.0.0.1";
  config.client_max_body_size = 10;

  HttpRequest req;
  req.setConfig(&config);

  // Content-Length: 10 (制限と同じ → OK)
  const char* raw =
      "POST /upload HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Content-Length: 10\r\n"
      "\r\n"
      "1234567890";

  bool completed = req.feed(raw, std::strlen(raw));

  printResult("BodyExactLimit: Parse Complete", completed == true);
  printResult("BodyExactLimit: No Error", req.hasError() == false);

  std::vector<char> body = req.getBody();
  std::string bodyStr(body.begin(), body.end());
  printResult("BodyExactLimit: Body Content", bodyStr == "1234567890");
}

// =============================================================================
// テスト13: Content-Length が client_max_body_size を1バイト超過
// =============================================================================
void test_BodyOneByteOver() {
  printSection("Body One Byte Over Test");

  ServerConfig config;
  config.listen_port = 8080;
  config.host = "127.0.0.1";
  config.client_max_body_size = 10;

  HttpRequest req;
  req.setConfig(&config);

  // Content-Length: 11 (制限の10を1バイト超過)
  const char* raw =
      "POST /upload HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Content-Length: 11\r\n"
      "\r\n";

  bool completed = req.feed(raw, std::strlen(raw));

  printResult("BodyOneByteOver: Not Complete", completed == false);
  printResult("BodyOneByteOver: Has Error", req.hasError() == true);
}

// =============================================================================
// テスト14: Configが設定されていない場合はデフォルト制限 (1MB) を使用
// =============================================================================
void test_NoConfigDefaultLimit() {
  printSection("No Config Default Limit Test");

  HttpRequest req;
  // setConfig() を呼ばない → デフォルト制限 (1MB = 1048576) が適用される

  // デフォルト制限内のContent-Length → OK
  const char* raw1 =
      "POST /upload HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Content-Length: 1000\r\n"
      "\r\n";

  bool completed1 = req.feed(raw1, std::strlen(raw1));
  printResult("NoConfigDefaultLimit: Small body OK (waiting for body)",
              completed1 == false);
  printResult("NoConfigDefaultLimit: No Error", req.hasError() == false);

  // リセットして再テスト
  req.clear();

  // デフォルト制限を超えるContent-Length → エラー
  const char* raw2 =
      "POST /upload HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Content-Length: 2000000\r\n"  // 2MB > 1MB
      "\r\n";

  bool completed2 = req.feed(raw2, std::strlen(raw2));
  printResult("NoConfigDefaultLimit: Large body rejected", completed2 == false);
  printResult("NoConfigDefaultLimit: Has Error", req.hasError() == true);
}

// =============================================================================
// テスト15: 基本的な chunked エンコーディング
// =============================================================================
void test_Chunked_Basic() {
  printSection("Chunked Basic Test");

  HttpRequest req;
  const char* raw =
      "POST /chunked HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n"
      "5\r\n"
      "Hello\r\n"
      "0\r\n"
      "\r\n";

  bool completed = req.feed(raw, std::strlen(raw));

  printResult("Chunked_Basic: Parse Complete", completed == true);
  printResult("Chunked_Basic: No Error", req.hasError() == false);

  std::vector<char> body = req.getBody();
  std::string bodyStr(body.begin(), body.end());
  printResult("Chunked_Basic: Body Content", bodyStr == "Hello");
}

// =============================================================================
// テスト16: 複数チャンク
// =============================================================================
void test_Chunked_MultipleChunks() {
  printSection("Chunked Multiple Chunks Test");

  HttpRequest req;
  const char* raw =
      "POST /multi HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n"
      "5\r\n"
      "Hello\r\n"
      "1\r\n"
      " \r\n"
      "6\r\n"
      "World!\r\n"
      "0\r\n"
      "\r\n";

  bool completed = req.feed(raw, std::strlen(raw));

  printResult("Chunked_Multiple: Parse Complete", completed == true);
  printResult("Chunked_Multiple: No Error", req.hasError() == false);

  std::vector<char> body = req.getBody();
  std::string bodyStr(body.begin(), body.end());
  printResult("Chunked_Multiple: Body Content", bodyStr == "Hello World!");
}

// =============================================================================
// テスト17: chunked の分割受信
// =============================================================================
void test_Chunked_Fragmented() {
  printSection("Chunked Fragmented Reception Test");

  HttpRequest req;

  // 1回目: ヘッダーとチャンクサイズの一部
  const char* chunk1 =
      "POST /frag HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n"
      "5\r\n"
      "Hel";

  bool done1 = req.feed(chunk1, std::strlen(chunk1));
  printResult("Chunked_Fragmented: Chunk 1 Incomplete", done1 == false);
  printResult("Chunked_Fragmented: No Error after chunk 1",
              req.hasError() == false);

  // 2回目: データの続きとCRLF
  const char* chunk2 = "lo\r\n";
  bool done2 = req.feed(chunk2, std::strlen(chunk2));
  printResult("Chunked_Fragmented: Chunk 2 Incomplete", done2 == false);

  // 3回目: 終端チャンク
  const char* chunk3 = "0\r\n\r\n";
  bool done3 = req.feed(chunk3, std::strlen(chunk3));
  printResult("Chunked_Fragmented: Chunk 3 Complete", done3 == true);

  std::vector<char> body = req.getBody();
  std::string bodyStr(body.begin(), body.end());
  printResult("Chunked_Fragmented: Body Content", bodyStr == "Hello");
}

// =============================================================================
// テスト18: chunk-extension 付き（セミコロン以降は無視）
// =============================================================================
void test_Chunked_WithExtension() {
  printSection("Chunked With Extension Test");

  HttpRequest req;
  const char* raw =
      "POST /ext HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n"
      "5;name=value\r\n"
      "Hello\r\n"
      "0;final\r\n"
      "\r\n";

  bool completed = req.feed(raw, std::strlen(raw));

  printResult("Chunked_Extension: Parse Complete", completed == true);
  printResult("Chunked_Extension: No Error", req.hasError() == false);

  std::vector<char> body = req.getBody();
  std::string bodyStr(body.begin(), body.end());
  printResult("Chunked_Extension: Body Content", bodyStr == "Hello");
}

// =============================================================================
// テスト19: trailer header 付き
// =============================================================================
void test_Chunked_WithTrailer() {
  printSection("Chunked With Trailer Test");

  HttpRequest req;
  const char* raw =
      "POST /trailer HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n"
      "5\r\n"
      "Hello\r\n"
      "0\r\n"
      "X-Checksum: abc123\r\n"
      "X-Another: value\r\n"
      "\r\n";

  bool completed = req.feed(raw, std::strlen(raw));

  printResult("Chunked_Trailer: Parse Complete", completed == true);
  printResult("Chunked_Trailer: No Error", req.hasError() == false);

  std::vector<char> body = req.getBody();
  std::string bodyStr(body.begin(), body.end());
  printResult("Chunked_Trailer: Body Content", bodyStr == "Hello");
}

// =============================================================================
// テスト20: chunked でボディサイズ制限を超過
// =============================================================================
void test_Chunked_BodyTooLarge() {
  printSection("Chunked Body Too Large Test");

  ServerConfig config;
  config.listen_port = 8080;
  config.host = "127.0.0.1";
  config.client_max_body_size = 10;

  HttpRequest req;
  req.setConfig(&config);

  // 制限10バイトに対して15バイト送信
  const char* raw =
      "POST /large HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n"
      "f\r\n"  // 15バイト
      "123456789012345\r\n"
      "0\r\n"
      "\r\n";

  bool completed = req.feed(raw, std::strlen(raw));

  printResult("Chunked_TooLarge: Not Complete", completed == false);
  printResult("Chunked_TooLarge: Has Error", req.hasError() == true);
}

// =============================================================================
// テスト21: chunked で累積サイズが制限を超過
// =============================================================================
void test_Chunked_CumulativeTooLarge() {
  printSection("Chunked Cumulative Too Large Test");

  ServerConfig config;
  config.listen_port = 8080;
  config.host = "127.0.0.1";
  config.client_max_body_size = 10;

  HttpRequest req;
  req.setConfig(&config);

  // 5バイト + 6バイト = 11バイト > 10バイト制限
  const char* raw =
      "POST /cumulative HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n"
      "5\r\n"
      "Hello\r\n"
      "6\r\n"  // この時点で累積11バイトになるのでエラー
      "World!\r\n"
      "0\r\n"
      "\r\n";

  bool completed = req.feed(raw, std::strlen(raw));

  printResult("Chunked_Cumulative: Not Complete", completed == false);
  printResult("Chunked_Cumulative: Has Error", req.hasError() == true);
}

// =============================================================================
// テスト22: 不正な16進数
// =============================================================================
void test_Chunked_InvalidHex() {
  printSection("Chunked Invalid Hex Test");

  HttpRequest req;
  const char* raw =
      "POST /invalid HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n"
      "xyz\r\n"  // 不正な16進数
      "Hello\r\n"
      "0\r\n"
      "\r\n";

  bool completed = req.feed(raw, std::strlen(raw));

  printResult("Chunked_InvalidHex: Not Complete", completed == false);
  printResult("Chunked_InvalidHex: Has Error", req.hasError() == true);
}

// =============================================================================
// テスト23: チャンクデータ後のCRLFがない
// =============================================================================
void test_Chunked_MissingCRLF() {
  printSection("Chunked Missing CRLF Test");

  HttpRequest req;
  const char* raw =
      "POST /missing HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n"
      "5\r\n"
      "HelloXX";  // \r\n ではなく XX

  bool completed = req.feed(raw, std::strlen(raw));

  printResult("Chunked_MissingCRLF: Not Complete", completed == false);
  printResult("Chunked_MissingCRLF: Has Error", req.hasError() == true);
}

// =============================================================================
// テスト24: 大文字の16進数
// =============================================================================
void test_Chunked_UppercaseHex() {
  printSection("Chunked Uppercase Hex Test");

  HttpRequest req;
  const char* raw =
      "POST /upper HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n"
      "A\r\n"  // 10バイト (大文字)
      "0123456789\r\n"
      "0\r\n"
      "\r\n";

  bool completed = req.feed(raw, std::strlen(raw));

  printResult("Chunked_UpperHex: Parse Complete", completed == true);
  printResult("Chunked_UpperHex: No Error", req.hasError() == false);

  std::vector<char> body = req.getBody();
  std::string bodyStr(body.begin(), body.end());
  printResult("Chunked_UpperHex: Body Content", bodyStr == "0123456789");
}

// =============================================================================
// テスト25: Transfer-Encoding の大文字小文字混在
// =============================================================================
void test_Chunked_CaseInsensitive() {
  printSection("Chunked Case Insensitive Test");

  HttpRequest req;
  const char* raw =
      "POST /case HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Transfer-Encoding: ChUnKeD\r\n"
      "\r\n"
      "5\r\n"
      "Hello\r\n"
      "0\r\n"
      "\r\n";

  bool completed = req.feed(raw, std::strlen(raw));

  printResult("Chunked_CaseInsensitive: Parse Complete", completed == true);
  printResult("Chunked_CaseInsensitive: No Error", req.hasError() == false);

  std::vector<char> body = req.getBody();
  std::string bodyStr(body.begin(), body.end());
  printResult("Chunked_CaseInsensitive: Body Content", bodyStr == "Hello");
}

// =============================================================================
// テスト26: chunked で空のボディ（終端チャンクのみ）
// =============================================================================
void test_Chunked_EmptyBody() {
  printSection("Chunked Empty Body Test");

  HttpRequest req;
  const char* raw =
      "POST /empty HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n"
      "0\r\n"
      "\r\n";

  bool completed = req.feed(raw, std::strlen(raw));

  printResult("Chunked_Empty: Parse Complete", completed == true);
  printResult("Chunked_Empty: No Error", req.hasError() == false);
  printResult("Chunked_Empty: Body Empty", req.getBody().empty());
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
  test_BodyTooLarge();
  test_BodyExactLimit();
  test_BodyOneByteOver();
  test_NoConfigDefaultLimit();

  // Chunked tests
  test_Chunked_Basic();
  test_Chunked_MultipleChunks();
  test_Chunked_Fragmented();
  test_Chunked_WithExtension();
  test_Chunked_WithTrailer();
  test_Chunked_BodyTooLarge();
  test_Chunked_CumulativeTooLarge();
  test_Chunked_InvalidHex();
  test_Chunked_MissingCRLF();
  test_Chunked_UppercaseHex();
  test_Chunked_CaseInsensitive();
  test_Chunked_EmptyBody();

  std::cout << GREEN << "\n=== All Body Parse Tests Passed! ===" << RESET
            << std::endl;
  return 0;
}
