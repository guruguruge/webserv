# webserv

C++98 で実装されたシンプルな HTTP サーバーです。

## ビルド

```bash
make
```

## 実行

```bash
./webserv config/default.conf
```

デフォルトではポート `8080` でリッスンします。

## 確認方法

### 1. ブラウザで確認

ブラウザで以下にアクセス：
```
http://localhost:8080
```

ウェルカムページが表示されます。

### 2. curl コマンドで確認

別のターミナルで以下を実行：

```bash
# GET リクエスト
curl http://localhost:8080

# 特定のファイルを取得
curl http://localhost:8080/test.txt

# アップロード一覧を確認
curl http://localhost:8080/upload

# ファイルをアップロード
curl -X POST -F "file=@/path/to/file.txt" http://localhost:8080/upload

# CGI スクリプト実行
curl http://localhost:8080/cgi-bin/test.py
```

### 3. ストレステスト

```bash
./test_stress.sh
```

## 設定ファイル

設定ファイルは `config/` ディレクトリにあります：
- `default.conf`: デフォルト設定（ポート 8080）
- `multi_server.conf`: 複数サーバー設定

## サーバー終了

ターミナルで `Ctrl+C` を入力します.

## イベントテスト

**ターミナル1**: サーバー起動
```bash
./webserv config/default.conf
```

**ターミナル2**: 各イベントをテスト

### 基本的な GET リクエスト
```bash
curl http://localhost:8080
```

### 複数の並行接続
```bash
# 複数の curl を同時に実行
for i in {1..5}; do curl http://localhost:8080 & done
wait
```

### POST リクエスト（ファイルアップロード）
```bash
# テストファイル作成
echo "テストデータ" > test_file.txt

# アップロード
curl -X POST -F "file=@test_file.txt" http://localhost:8080/upload
```

### DELETE リクエスト
```bash
# アップロード済みファイルを削除
curl -X DELETE http://localhost:8080/upload/test_file.txt
```

### CGI 実行
```bash
curl http://localhost:8080/cgi-bin/test.py
```

### タイムアウト（keep-alive）テスト
```bash
# 接続を保持して複数のリクエスト
curl -v http://localhost:8080
curl -v http://localhost:8080/test.txt
```

### エラーハンドリング
```bash
# 404 エラー
curl http://localhost:8080/notfound.html

# 500 エラー（存在しない CGI）
curl http://localhost:8080/cgi-bin/missing.py
```

### ストレステスト
```bash
./test_stress.sh
```

### 大きなファイル送信
```bash
# 1MB のテストファイル作成
dd if=/dev/zero of=large_file.bin bs=1M count=1

# アップロード
curl -X POST -F "file=@large_file.bin" http://localhost:8080/upload
```