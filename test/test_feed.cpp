#include "../inc/Http.hpp"
#include <cassert>
#include <cstring>
#include <iostream>

// =============================================================================
// テストユーティリティ
// =============================================================================
static int g_testCount = 0;
static int g_passCount = 0;

void printResult(const char* testName, bool passed) {
    g_testCount++;
    if (passed) {
        g_passCount++;
        std::cout << "[PASS] " << testName << std::endl;
    } else {
        std::cout << "[FAIL] " << testName << std::endl;
    }
}

// =============================================================================
// 初期状態のテスト
// =============================================================================
void test_initial_state() {
    HttpRequest req;
    
    bool passed = true;
    passed = passed && !req.isComplete();
    passed = passed && !req.hasError();
    passed = passed && (req.getMethod() == UNKNOWN_METHOD);
    passed = passed && (req.getPath() == "");
    
    printResult("initial_state", passed);
}

// =============================================================================
// 空データをfeedするテスト
// =============================================================================
void test_feed_empty() {
    HttpRequest req;
    
    bool result = req.feed("", 0);
    
    bool passed = true;
    passed = passed && !result;         // 空データでは完了しない
    passed = passed && !req.hasError(); // エラーにもならない
    passed = passed && !req.isComplete();
    
    printResult("feed_empty", passed);
}

// =============================================================================
// 部分的なリクエストライン（改行なし）
// =============================================================================
void test_feed_partial_request_line() {
    HttpRequest req;
    
    const char* data = "GET /index.html";
    bool result = req.feed(data, std::strlen(data));
    
    bool passed = true;
    passed = passed && !result;           // まだ完了しない
    passed = passed && !req.isComplete();
    
    printResult("feed_partial_request_line", passed);
}

// =============================================================================
// 複数回feedするテスト（データが分割されて届く場合）
// =============================================================================
void test_feed_multiple_chunks() {
    HttpRequest req;
    
    // 1回目: メソッドとパスの一部
    const char* chunk1 = "GET /in";
    bool result1 = req.feed(chunk1, std::strlen(chunk1));
    
    // 2回目: パスの残り
    const char* chunk2 = "dex.html";
    bool result2 = req.feed(chunk2, std::strlen(chunk2));
    
    bool passed = true;
    passed = passed && !result1;          // まだ完了しない
    passed = passed && !result2;          // まだ完了しない
    passed = passed && !req.isComplete();
    
    printResult("feed_multiple_chunks", passed);
}

// =============================================================================
// clearのテスト（Keep-Alive用リセット）
// =============================================================================
void test_clear() {
    HttpRequest req;
    
    // 何かデータをfeed
    const char* data = "GET /test";
    req.feed(data, std::strlen(data));
    
    // クリア
    req.clear();
    
    bool passed = true;
    passed = passed && !req.isComplete();
    passed = passed && !req.hasError();
    passed = passed && (req.getMethod() == UNKNOWN_METHOD);
    passed = passed && (req.getPath() == "");
    
    printResult("clear", passed);
}

// =============================================================================
// parseRequestLine実装後に通るべきテスト（現状はスキップ）
// =============================================================================
void test_complete_request_line() {
    HttpRequest req;
    
    // 完全なリクエストライン + ヘッダー終端
    const char* data = "GET /index.html HTTP/1.1\r\n\r\n";
    bool result = req.feed(data, std::strlen(data));
    
    // TODO: parseRequestLine実装後、これらが通るようになる
    // 現状はパース関数が空なので isComplete() = false
    std::cout << "[INFO] complete_request_line: result=" << result 
              << ", isComplete=" << req.isComplete() << " (expected after implementation)" << std::endl;
}

// =============================================================================
// メイン
// =============================================================================
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   HttpRequest::feed() テスト" << std::endl;
    std::cout << "========================================" << std::endl;
    
    test_initial_state();
    test_feed_empty();
    test_feed_partial_request_line();
    test_feed_multiple_chunks();
    test_clear();
    
    std::cout << "----------------------------------------" << std::endl;
    test_complete_request_line();
    std::cout << "----------------------------------------" << std::endl;
    
    std::cout << std::endl;
    std::cout << "Result: " << g_passCount << "/" << g_testCount << " passed" << std::endl;
    
    if (g_passCount == g_testCount) {
        std::cout << "\n=== All tests passed! ===" << std::endl;
        return 0;
    } else {
        std::cout << "\n=== Some tests failed ===" << std::endl;
        return 1;
    }
}
