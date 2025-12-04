#include <iostream>
#include <string>
#include <vector>
#include "Http.hpp"

#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"

void assert_eq(const std::string& test_name, size_t actual, size_t expected) {
  if (actual == expected) {
    std::cout << GREEN << "[OK] " << test_name << ": " << actual << RESET
              << std::endl;
  } else {
    std::cout << RED << "[NG] " << test_name << ": Expected " << expected
              << ", but got " << actual << RESET << std::endl;
  }
}

void assert_bool(const std::string& test_name, bool condition) {
  if (condition) {
    std::cout << GREEN << "[OK] " << test_name << RESET << std::endl;
  } else {
    std::cout << RED << "[NG] " << test_name << RESET << std::endl;
  }
}

int main() {
  std::cout << "=== PR3: State Management & Overflow Protection Test ==="
            << std::endl;

  // -------------------------------------------------------------------------
  // テストケース 1: 正常な段階的送信 (Step-by-step sending)
  // -------------------------------------------------------------------------
  {
    std::cout << "\n--- TEST 1: Normal Advance ---" << std::endl;
    HttpResponse res;

    // 分かりやすいように "0123456789" (10バイト) をBodyにする
    // ※ build() でヘッダーが付くので実際のサイズはもっと増えます
    res.setBody("0123456789");
    res.build();

    size_t total_size = res.getRemainingSize();
    std::cout << "Total Response Size: " << total_size << " bytes" << std::endl;

    // ステップ1: 最初の1バイトも送信していない状態
    const char* ptr_start = res.getData();
    assert_bool("Initial pointer is not NULL", ptr_start != NULL);
    assert_eq("Initial remaining size", res.getRemainingSize(), total_size);
    assert_bool("Initial isDone is false", !res.isDone());

    // ステップ2: 5バイト進める
    std::cout << ">> advance(5)" << std::endl;
    res.advance(5);

    const char* ptr_step1 = res.getData();
    assert_eq("Remaining after 5 bytes", res.getRemainingSize(),
              total_size - 5);
    assert_bool("Pointer advanced correctly", ptr_step1 == ptr_start + 5);

    // ステップ3: 残りを全部進める
    std::cout << ">> advance(rest)" << std::endl;
    res.advance(total_size - 5);

    assert_eq("Remaining should be 0", res.getRemainingSize(), 0);
    assert_bool("isDone should be true", res.isDone());

    // 完了後の getData() は NULL を返すべき（安全策）
    assert_bool("getData() returns NULL after done", res.getData() == NULL);
  }

  // -------------------------------------------------------------------------
  // テストケース 2: オーバーフローガード (Overflow Protection)
  // -------------------------------------------------------------------------
  {
    std::cout << "\n--- TEST 2: Overflow Guard ---" << std::endl;
    HttpResponse res;
    res.setBody("Short");
    res.build();

    // いきなり巨大な値を渡す（バッファサイズより大きい値）
    std::cout << ">> advance(999999) [Larger than total size]" << std::endl;
    res.advance(999999);

    // ガードが効いていれば、サイズぴったりで止まるはず
    assert_eq("Sent bytes capped at total size", res.getRemainingSize(), 0);
    assert_bool("isDone becomes true safely", res.isDone());
  }

  // -------------------------------------------------------------------------
  // テストケース 3: 空レスポンスの挙動 (Edge Case)
  // -------------------------------------------------------------------------
  {
    std::cout << "\n--- TEST 3: Empty Response ---" << std::endl;
    HttpResponse res;
    res.clear();
    // build()を呼ばない、または空の状態

    // 空の状態での挙動確認
    // ※実装によっては build() 前の _responseBuffer は空なので NULL が返るはず
    assert_bool("Empty buffer returns NULL data", res.getData() == NULL);
    assert_bool("Empty buffer isDone is true (or handle safely)",
                res.getRemainingSize() == 0 || res.isDone());
  }

  return 0;
}
