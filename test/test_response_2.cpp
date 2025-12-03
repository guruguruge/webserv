#include <iostream>
#include <string>
#include <vector>
#include "Http.hpp"

// 【重要】この関数のプロトタイプ宣言を Http.hpp に friend として追加します
void inspectBuffer(const HttpResponse& res) {
  const std::vector<char>& buffer = res._responseBuffer;

  std::cout << "--- [BUFFER START] ---" << std::endl;
  for (size_t i = 0; i < buffer.size(); ++i) {
    char c = buffer[i];
    if (c == '\r')
      std::cout << "\\r";
    else if (c == '\n')
      std::cout << "\\n\n";
    else if (std::isprint(c))
      std::cout << c;
    else
      std::cout << ".";
  }
  std::cout << "--- [BUFFER END] ---" << std::endl;
}

int main() {
  std::cout << "=== PR2: Build Logic Test (via friend) ===" << std::endl;

  try {
    HttpResponse res;

    // 1. データをセット
    res.setStatusCode(200);
    res.setHeader("Content-Type", "text/html");
    res.setHeader("Server", "Webserv/1.0");
    res.setBody("<html><body>Hello</body></html>");

    // 2. ビルド実行
    res.build();
    std::cout << "Build called." << std::endl;

    // 3. 結果確認 (friend関数を使用)
    inspectBuffer(res);

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
