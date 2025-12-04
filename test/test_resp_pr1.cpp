#include <iostream>
#include <vector>

#include "Http.hpp"

// 色付き出力用のマクロ（見やすくするため）
#define GREEN "\033[32m"
#define RESET "\033[0m"

void print_test_name(const std::string& name) {
  std::cout << "\n[TEST] " << name << "..." << std::endl;
}

int main() {
  std::cout << "=== PR1: HttpResponse Skeleton & Setters Test ===" << std::endl;

  try {
    // ---------------------------------------------------------
    // 1. インスタンス生成とデフォルト状態の確認
    // ---------------------------------------------------------
    print_test_name("Constructor & Defaults");
    HttpResponse res;
    std::cout << "  Instance created successfully." << std::endl;

    // ---------------------------------------------------------
    // 2. 基本的なセッターの動作確認
    // ---------------------------------------------------------
    print_test_name("Basic Setters");

    // ステータスコード (内部でstatic mapが初期化されるはず)
    res.setStatusCode(404);
    std::cout << "  setStatusCode(404) called." << std::endl;

    // ヘッダー設定
    res.setHeader("Content-Type", "text/html");
    res.setHeader("Server", "webserv/1.0");
    std::cout << "  setHeader called." << std::endl;

    // ボディ設定 (std::string版)
    res.setBody("<html><body>Not Found</body></html>");
    std::cout << "  setBody(string) called." << std::endl;

    // ボディ設定 (std::vector<char>版)
    std::string raw_data = "BinaryData\0WithNull";
    std::vector<char> vec_body(raw_data.begin(), raw_data.end());
    res.setBody(vec_body);
    std::cout << "  setBody(vector) called." << std::endl;

    // ---------------------------------------------------------
    // 3. Orthodox Canonical Form の確認 (コピーと代入)
    // ---------------------------------------------------------
    print_test_name("Orthodox Canonical Form");

    // コピーコンストラクタ
    HttpResponse res_copy(res);
    std::cout << "  Copy Constructor called." << std::endl;

    // 代入演算子
    HttpResponse res_assign;
    res_assign = res;
    std::cout << "  Assignment Operator called." << std::endl;

    // 自己代入チェック (これがクラッシュしないか)
    HttpResponse* ptr = &res;
    res = *ptr;
    std::cout << "  Self-Assignment check passed." << std::endl;

    // ---------------------------------------------------------
    // 4. Clear関数の動作確認
    // ---------------------------------------------------------
    print_test_name("Clear Function");
    res.clear();
    std::cout << "  clear() called." << std::endl;

    // クリア後に再利用しても大丈夫か
    res.setStatusCode(200);
    res.setBody("OK");
    std::cout << "  Re-use after clear passed." << std::endl;

    // ---------------------------------------------------------
    // 5. プレースホルダー関数の呼び出し確認
    //    (中身が空でも呼んで落ちないかチェック)
    // ---------------------------------------------------------
    print_test_name("Placeholder Methods");
    res.build();
    res.getData();
    res.getRemainingSize();
    res.advance(10);
    res.isDone();
    // res.setBodyFile("dummy.txt"); // TODOコメントがあるので呼んでも何も起きないはず
    std::cout << "  All placeholders called without crash." << std::endl;

  } catch (const std::exception& e) {
    std::cerr << "\n[ERROR] Exception caught: " << e.what() << std::endl;
    return 1;
  } catch (...) {
    std::cerr << "\n[ERROR] Unknown exception caught." << std::endl;
    return 1;
  }

  std::cout << GREEN << "\n=== ALL TESTS PASSED (PR1) ===" << RESET
            << std::endl;
  return 0;
}
