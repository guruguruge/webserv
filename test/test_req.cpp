#include "../inc/Http.hpp"
#include "../inc/Config.hpp"
#include <iostream>
#include <cassert>
#include <cstring>

// 色付き出力用
#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"

void printResult(const std::string &testName, bool result)
{
    if (result)
    {
        std::cout << GREEN << "[PASS] " << testName << RESET << std::endl;
    }
    else
    {
        std::cout << RED << "[FAIL] " << testName << RESET << std::endl;
        exit(1);
    }
}

// テスト1: 標準的なGETリクエスト（一括受信）
void test_SimpleGet()
{
    HttpRequest req;
    const char *raw =
        "GET /index.html HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "User-Agent: curl/7.64.1\r\n"
        "Accept: */*\r\n"
        "\r\n";

    bool completed = req.feed(raw, std::strlen(raw));

    printResult("SimpleGet: Parse Complete", completed == true);
    printResult("SimpleGet: Method check", req.getMethod() == GET);
    printResult("SimpleGet: Path check", req.getPath() == "/index.html");
    printResult("SimpleGet: Header check", req.getHeader("Host") == "localhost:8080");
}

// テスト2: 分割受信（epollでパケットが分かれたシミュレーション）
void test_FragmentedRequest()
{
    HttpRequest req;

    // 1回目: ヘッダーの途中まで
    const char *chunk1 =
        "POST /submit HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "He"; // Bodyの途中

    bool done1 = req.feed(chunk1, std::strlen(chunk1));
    printResult("Fragmented: Chunk 1 Incomplete", done1 == false);

    // 2回目: 残りのBody
    const char *chunk2 = "llo";

    bool done2 = req.feed(chunk2, std::strlen(chunk2));
    printResult("Fragmented: Chunk 2 Complete", done2 == true);

    // Bodyの検証 (vector<char> -> string変換)
    std::vector<char> body = req.getBody();
    std::string bodyStr(body.begin(), body.end());

    printResult("Fragmented: Method POST", req.getMethod() == POST);
    printResult("Fragmented: Body Content", bodyStr == "Hello");
}

int main()
{
    std::cout << "=== Running HttpRequest Tests ===" << std::endl;

    test_SimpleGet();
    test_FragmentedRequest();

    std::cout << "All HttpRequest tests passed!" << std::endl;
    return 0;
}