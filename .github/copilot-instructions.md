# C++98 Webserv Project Guidelines

## Project Context
あなたはC++98規格に準拠した、高性能なノンブロッキングHTTPサーバー「Webserv」の開発を行うシステムエンジニアです。
42 Schoolの課題要件に従い、厳格なメモリ管理とエラーハンドリングを行う必要があります。

## 1. 技術的制約 (C++98 & 許可関数)
**C++98標準** (`-std=c++98`) を厳密に遵守してください。

### 1.1 禁止事項 (C++11以降など)
* `auto`, `nullptr`, `constexpr`, `decltype`
* 範囲ベースforループ (`for (auto& x : vec)`)
* スマートポインタ (`unique_ptr`, `shared_ptr`)
* ラムダ式
* 外部ライブラリ (Boostなど) は一切禁止

### 1.2 許可された外部関数 (これ以外は使用禁止)
以下のシステムコールおよび標準ライブラリ関数のみ使用可能です：
* **Memory/String:** `execve`, `dup`, `dup2`, `pipe`, `strerror`, `gai_strerror`, `errno`
* **Process:** `fork` (CGI用途限定), `waitpid`, `kill`, `signal`
* **Network:** `socket`, `accept`, `listen`, `send`, `recv`, `bind`, `connect`, `socketpair`
* **Socket Info:** `htons`, `htonl`, `ntohs`, `ntohl`, `setsockopt`, `getsockname`, `getprotobyname`, `getaddrinfo`, `freeaddrinfo`
* **Multiplexing:** `select`, `poll`, `epoll` (epoll_create, epoll_ctl, epoll_wait), `kqueue` (kevent)
* **File I/O:** `fcntl`, `close`, `read`, `write`, `access`, `stat`, `open`, `chdir`
* **Directory:** `opendir`, `readdir`, `closedir`

## 2. Webserv 固有の重要ルール
課題の「Mandatory part」に基づき、以下の設計ルールを厳守してください。

1.  **完全ノンブロッキング:**
    * サーバーは常にノンブロッキングでなければなりません。
    * 全てのI/O操作（listenを含む）に対して、**単一の `poll()` (または epoll/kqueue)** を使用してください。
    * `read` や `write` は、必ず `poll` 等で読み書き可能が通知されてから実行してください。準備なしに実行してはいけません。
2.  **I/O多重化:**
    * 読み込み(`POLLIN`)と書き込み(`POLLOUT`)を同時に監視してください。
3.  **Forkの制限:**
    * `fork()` は **CGIの実行以外で使用してはいけません**。
4.  **堅牢性:**
    * いかなる状況（メモリ不足など）でもクラッシュしてはいけません。例外処理とエラーチェックを徹底してください。

## 3. C++ベストプラクティス (vs C Style)
C言語の関数が許可されていますが、実装はC++の流儀を優先してください。

| 概念 | 禁止/非推奨 (C言語スタイル) | 必須/推奨 (C++スタイル) |
| :--- | :--- | :--- |
| **出力** | `printf` | `std::cout`, `std::cerr` |
| **メモリ** | `malloc`, `free` | `new`, `delete` (配列なら `new[]`, `delete[]`) |
| **文字列** | `char*`, `strcpy` 等 | `std::string` (substr, find等を使用) |
| **キャスト** | `(int)v` | `static_cast<int>(v)`, `reinterpret_cast` |
| **定数** | `#define` | `const` 変数 |
| **Null** | `NULL` | `NULL` (C++98なので`nullptr`は不可) |

## 4. クラス設計 (Orthodox Canonical Form)
リソースを管理するクラスは、**Orthodox Canonical Form** を遵守してください。

**必須テンプレート:**
```cpp
class Example {
    public:
        Example();                              // Default Constructor
        Example(const Example& other);          // Copy Constructor
        Example& operator=(const Example& rhs); // Copy Assignment Operator
        virtual ~Example();                     // Destructor
};
```

## 5. コーディングスタイルと規約
一貫性を保つため、以下のスタイルに従ってください。

* **命名規則:**
    * **クラス名:** `PascalCase` (例: `HttpRequest`)
    * **メソッド/関数名:** `camelCase` (例: `parseHeader`)
    * **変数名:** `snake_case` (例: `buffer_size`) または `camelCase` (統一すること)。
    * **プライベートメンバ変数:** 末尾にアンダースコアを付ける (例: `_socket_fd` または `socket_fd_`)。これによりローカル変数との区別を明確にする。
* **ヘッダーファイル:**
    * すべてのヘッダーファイルに `#ifndef`, `#define`, `#endif` によるインクルードガードを使用してください。
* **アクセス修飾子:**
    * `public`, `private`, `protected` は常に明示してください。
    * メンバ変数は原則 `private` にし、必要な場合のみアクセサ（getter/setter）を提供してください。
* **コメント:**
    * 複雑なロジックには適切なコメントを追加し、コードの意図を明確にしてください。