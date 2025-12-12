# NGINX Reference Environment for Webserv

## 概要

このディレクトリは、**Webservプロジェクトの「正解」挙動を確認するための参照環境**です。

本物のNGINXをDockerで起動し、以下を確認できます：
- HTTP/1.0 と HTTP/1.1 の違い
- `Host` ヘッダーの有無による挙動（400 Bad Request）
- autoindex（ディレクトリリスティング）の表示形式
- エラーページ（404など）の応答
- レスポンスヘッダーの形式

Webservの実装が正しいかどうかを、NGINXの実際の挙動と比較することで検証できます。

---

## ディレクトリ構成

```
nginx-test/
├── docker-compose.yml   # NGINXコンテナの起動設定
├── nginx.conf           # NGINXの設定ファイル
├── html/                # 配信するHTMLファイル
│   ├── index.html       # トップページ
│   └── empty_folder/    # autoindex確認用ディレクトリ
│       └── test.txt
└── README.md            # このファイル
```

---

## 準備 (Setup)

### 必要なもの
- Docker
- Docker Compose

### ファイル説明

#### `docker-compose.yml`
```yaml
version: '3'
services:
  nginx-ref:
    image: nginx:alpine
    container_name: nginx-reference
    ports:
      - "8080:8080"  # Webservと同じポート
    volumes:
      - ./nginx.conf:/etc/nginx/nginx.conf:ro
      - ./html:/var/www/html:ro
```

#### `nginx.conf`
- `autoindex on`: ディレクトリリスティングを有効化
- `error_page 404`: 404エラーページの設定
- `listen 8080`: Webservと同じポート番号

#### `html/index.html`
テスト用のシンプルなHTMLファイル

---

## 起動 (Start)

### NGINXコンテナを起動
```bash
cd nginx-test
docker compose up
```

**バックグラウンドで起動する場合:**
```bash
docker compose up -d
```

### 起動確認
```bash
curl http://localhost:8080
```

以下のようなレスポンスが返ればOK:
```html
<h1>Hello from NGINX Reference!</h1>
<p>Use this to compare headers.</p>
```

### 停止
```bash
docker compose down
```

---

## テスト方法 (Test Methods)

### 1. curl でのテスト

#### HTTP/1.1 リクエスト（デフォルト）
```bash
curl -v http://localhost:8080
```

**確認ポイント:**
- レスポンスヘッダー: `Connection: keep-alive`
- HTTP/1.1 では `Host` ヘッダーが自動的に付与される

#### HTTP/1.0 リクエスト
```bash
curl -v --http1.0 http://localhost:8080
```

**確認ポイント:**
- レスポンスヘッダー: `Connection: close`
- HTTP/1.0 でも `Host` ヘッダーは curl が付与する

#### Host ヘッダーなしリクエスト（telnet使用）
後述の telnet セクションを参照

#### レスポンスヘッダーのみ取得
```bash
curl -I http://localhost:8080
```

---

### 2. telnet でのテスト

telnetを使うと、**生のHTTPリクエスト**を手動で送信できます。

#### HTTP/1.1 で Host ヘッダーあり（正常）
```bash
telnet localhost 8080
```

接続後、以下を**手入力**（最後に2回Enterを押す）:
```http
GET / HTTP/1.1
Host: localhost

```

**結果:** 200 OK が返る

#### HTTP/1.1 で Host ヘッダーなし（エラー）
```bash
telnet localhost 8080
```

接続後、以下を入力:
```http
GET / HTTP/1.1

```

**結果:** `400 Bad Request` が返る
- HTTP/1.1 では `Host` ヘッダーが**必須**

#### HTTP/1.0 で Host ヘッダーなし（正常）
```bash
telnet localhost 8080
```

接続後、以下を入力:
```http
GET / HTTP/1.0

```

**結果:** 200 OK が返る
- HTTP/1.0 では `Host` ヘッダーは不要

---

### 3. ブラウザでのテスト

#### インデックスページの表示
ブラウザで以下にアクセス:
```
http://localhost:8080
```

→ `index.html` の内容が表示される

#### autoindex（ディレクトリリスティング）の確認
ブラウザで以下にアクセス:
```
http://localhost:8080/empty_folder/
```

→ NGINXのディレクトリリスティングが表示される
- ファイル名のリスト
- ファイルサイズ
- 最終更新日時

**Webservの実装時の参考:**
- HTMLの形式
- ヘッダーの書き方
- リンクの形式

#### 404エラーページの確認
```
http://localhost:8080/notfound.html
```

→ 404 エラーページが表示される

---

## 設定変更とテスト

### nginx.conf を変更する

例: autoindex を無効化
```nginx
location / {
    autoindex off;  # on から off に変更
    try_files $uri $uri/ =404;
}
```

### 変更を反映
```bash
docker compose restart
```

### 確認
```bash
curl http://localhost:8080/empty_folder/
```

→ autoindex が off なら 403 Forbidden が返る

---

## よくあるテストシナリオ

### シナリオ1: レスポンスヘッダーの確認
```bash
curl -I http://localhost:8080
```

**確認項目:**
- `Server: nginx/1.x.x`
- `Content-Type: text/html`
- `Content-Length: xxx`
- `Connection: keep-alive`

### シナリオ2: Keep-Alive の挙動
```bash
curl -v http://localhost:8080 http://localhost:8080
```

→ 同じ接続が再利用されるか確認

### シナリオ3: POSTリクエスト
```bash
curl -X POST http://localhost:8080/upload
```

→ 405 Method Not Allowed など、許可されていないメソッドへの応答を確認

### シナリオ4: 大きなファイルの配信
大きなファイルを `html/` に配置して:
```bash
curl -o /dev/null http://localhost:8080/large_file.bin
```

→ チャンク転送などの挙動を確認

---

## Webserv との比較方法

### 1. 同じリクエストを両方に送る

**NGINX (参照):**
```bash
curl -v http://localhost:8080
```

**Webserv:**
```bash
curl -v http://localhost:8080
```

※ Webserv も同じポートで起動している場合は、NGINXを停止してから起動

### 2. レスポンスヘッダーを比較
```bash
# NGINX
curl -I http://localhost:8080 > nginx_headers.txt

# Webserv
curl -I http://localhost:8080 > webserv_headers.txt

# 差分確認
diff nginx_headers.txt webserv_headers.txt
```

### 3. autoindex のHTML形式を比較
```bash
# NGINX
curl http://localhost:8080/empty_folder/ > nginx_autoindex.html

# Webserv
curl http://localhost:8080/empty_folder/ > webserv_autoindex.html

# ブラウザで開いて見た目を比較
```

---

### 設定が反映されない
```bash
# コンテナを完全に削除して再起動
docker compose down
docker compose up
```

### ログを確認したい
```bash
# リアルタイムでログを表示
docker compose logs -f

# NGINXのエラーログを確認
docker exec nginx-reference cat /var/log/nginx/error.log
```

---

## 参考リソース

- [NGINX公式ドキュメント](https://nginx.org/en/docs/)
- [HTTP/1.1 RFC 2616](https://www.rfc-editor.org/rfc/rfc2616)
- [HTTP/1.0 RFC 1945](https://www.rfc-editor.org/rfc/rfc1945)

---

## まとめ

この環境を使うことで:
✅ NGINXの正確な挙動を確認できる  
✅ Webservの実装が正しいか検証できる  
✅ HTTP仕様の理解が深まる  

テストを繰り返して、Webservの品質を高めましょう！
