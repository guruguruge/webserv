#include <cassert>
#include <cstring>
#include <iostream>
#include "../inc/Config.hpp"
#include "../inc/Http.hpp"
// 色付き出力用
#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"

void printResult(const std::string& testName, bool result) {
  if (result) {
    std::cout << GREEN << "[PASS] " << testName << RESET << std::endl;
  } else {
    std::cout << RED << "[FAIL] " << testName << RESET << std::endl;
    exit(1);
  }
}

// テスト1: 標準的なGETリクエスト（一括受信）
void test_SimpleGet() {
  HttpRequest req;
  const char* raw =
      "GET /index.html HTTP/1.1\r\n"
      "Host: localhost:8080\r\n"
      "User-Agent: curl/7.64.1\r\n"
      "Accept: */*\r\n"
      "\r\n";

  bool completed = req.feed(raw, std::strlen(raw));

  printResult("SimpleGet: Parse Complete", completed == true);
  printResult("SimpleGet: Method check", req.getMethod() == GET);
  printResult("SimpleGet: Path check", req.getPath() == "/index.html");
  printResult("SimpleGet: Header check",
              req.getHeader("Host") == "localhost:8080");
}

// テスト2: 分割受信（epollでパケットが分かれたシミュレーション）
void test_FragmentedRequest() {
  HttpRequest req;

  // 1回目: ヘッダーの途中まで
  const char* chunk1 =
      "POST /submit HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "Content-Length: 5\r\n"
      "\r\n"
      "He";  // Bodyの途中

  bool done1 = req.feed(chunk1, std::strlen(chunk1));
  printResult("Fragmented: Chunk 1 Incomplete", done1 == false);

  // 2回目: 残りのBody
  const char* chunk2 = "llo";

  bool done2 = req.feed(chunk2, std::strlen(chunk2));
  printResult("Fragmented: Chunk 2 Complete", done2 == true);

  // Bodyの検証 (vector<char> -> string変換)
  std::vector<char> body = req.getBody();
  std::string bodyStr(body.begin(), body.end());

  printResult("Fragmented: Method POST", req.getMethod() == POST);
  printResult("Fragmented: Body Content", bodyStr == "Hello");
}

int main() {
  std::cout << "=== Running HttpRequest Tests ===" << std::endl;

  test_SimpleGet();
  test_FragmentedRequest();

  std::cout << "All HttpRequest tests passed!" << std::endl;
  return 0;
}

// 1. test_HttpRequest.cpp 用 (受信・パース) sabe
// A. 正常系: 基本メソッドと完全なリクエスト
// まずは普通にデータが一度に来た場合です。
//     test_Parse_SimpleGet(): 標準的なGETリクエストがパースできるか。
//     test_Parse_PostWithContentLength(): Content-Length 指定のPOSTで、Bodyが正しく読み取れるか。
//     test_Parse_Delete(): DELETEリクエストがパースできるか。
//     test_Parse_Head(): HEADリクエスト（Bodyなし）が処理できるか。
//     test_Header_CaseInsensitivity(): Content-Type と content-type を同一視できるか。
//     test_Header_Whitespace(): Host: localhost のようにコロンの後にスペースがあってもパースできるか。
// B. 正常系: 分割受信 (epoll/ノンブロッキング対応)
// ここが最重要です。 feed() を細かく刻んで呼び出します。
//     test_Partial_RequestLine(): GET /ind ... (遅延) ... ex.html HTTP/1.1 のようにリクエストラインが分割されても耐えられるか。
//     test_Partial_Headers(): ヘッダーの途中でデータが途切れても、次が来たら再開できるか。
//     test_Partial_Body_ContentLength(): 指定されたLength分のデータが、3回くらいに分かれて届いても結合できるか。
//     test_Boundary_CrLf(): \r と \n の間でパケットが分割された場合（\r だけ届いて \n がまだの状態）にバグらないか。
// C. 準正常系: 特殊なエンコーディング
//     test_Parse_ChunkedBody_Simple(): Transfer-Encoding: chunked が正しくパースできるか。
//     test_Parse_ChunkedBody_Split(): チャンクサイズの数字やデータ本体が分割受信されても平気か。
//     test_Parse_ChunkedBody_ZeroChunk(): 最後の 0\r\n\r\n を検知して終了できるか。
//     test_Body_BinaryData(): 画像データなど、途中に NULL文字 (\0) を含むBodyを std::string ではなく vector<char> として正しく保持できているか。
// D. 異常系: エラーハンドリング (4xx系)
//     test_Error_UriTooLong(): MAX_URI_LENGTH を超えるパスが来た時にエラー状態になるか。
//     test_Error_HeaderTooLarge(): ヘッダーサイズが巨大すぎる場合に拒否できるか。
//     test_Error_InvalidMethod(): HOGE / HTTP/1.1 のような不明なメソッドを弾けるか。
//     test_Error_InvalidHttpVersion(): HTTP/1.0 や HTTP/3 が来た時の挙動（許容するか弾くか）。
//     test_Error_MissingHostHeader(): HTTP/1.1 なのに Host ヘッダーがない場合にエラーにできるか。
//     test_Error_ContentLength_Format(): Content-Length: abc や負の値などの不正フォーマット。