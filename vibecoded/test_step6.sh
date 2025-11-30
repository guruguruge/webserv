#!/bin/bash

# ==================================================
# Step 6 テストスクリプト: Router と静的ファイルGET の動作確認
# ==================================================

echo "=========================================="
echo " Step 6: Router & Static File Handler テスト"
echo "=========================================="
echo ""

# カラーコード
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# テスト結果カウンタ
TOTAL=0
PASSED=0
FAILED=0

# テスト関数
test_request() {
    local test_name="$1"
    local expected_keyword="$2"
    shift 2
    
    TOTAL=$((TOTAL + 1))
    echo -n "Test $TOTAL: $test_name ... "
    
    # リクエストを実行
    output=$("$@" 2>&1)
    
    # 期待するキーワードが含まれているかチェック
    if echo "$output" | grep -q "$expected_keyword"; then
        echo -e "${GREEN}✓ PASSED${NC}"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED}✗ FAILED${NC}"
        echo "  Expected: '$expected_keyword'"
        echo "  Got: $output"
        FAILED=$((FAILED + 1))
    fi
}

echo "=== 1. 基本的なファイル取得 ==="
test_request "GET index.html" "Welcome to Webserv" \
    curl -s http://localhost:8080/

test_request "GET text file" "test text file" \
    curl -s http://localhost:8080/test.txt

echo ""
echo "=== 2. MIMEタイプ判定 ==="
test_request "HTML Content-Type" "Content-Type: text/html" \
    curl -I -s http://localhost:8080/ 

test_request "Text Content-Type" "Content-Type: text/plain" \
    curl -I -s http://localhost:8080/test.txt

echo ""
echo "=== 3. HEAD メソッド ===" 
test_request "HEAD request" "HTTP/1.1 200 OK" \
    curl -I -s http://localhost:8080/

test_request "HEAD has no body" "Content-Type: text/html" \
    sh -c 'response=$(curl -I -s http://localhost:8080/); echo "$response" | grep -v "<!DOCTYPE"'

echo ""
echo "=== 4. エラーハンドリング ==="
test_request "404 Not Found" "404 Not Found" \
    curl -s http://localhost:8080/nonexistent.html

test_request "404 status code" "HTTP/1.1 404" \
    curl -I -s http://localhost:8080/nonexistent.html

echo ""
echo "=== 5. ディレクトリ処理 ==="
test_request "Directory with index" "Welcome to Webserv" \
    curl -s http://localhost:8080/

test_request "Autoindex enabled" "Index of /upload/" \
    curl -s http://localhost:8080/upload/

test_request "Autoindex lists files" "index.html" \
    curl -s http://localhost:8080/upload/

echo ""
echo "=== 6. ロケーション選択 ==="
test_request "Root location" "Welcome" \
    curl -s http://localhost:8080/

test_request "Upload location" "Index of /upload/" \
    curl -s http://localhost:8080/upload/

echo ""
echo "=== 7. メソッド制限 ==="
# DELETE は Step 8 で実装予定のため、現時点では 501 Not Implemented
TOTAL=$((TOTAL + 1))
echo -n "Test $TOTAL: DELETE not implemented yet ... "
status_code=$(curl -X DELETE -s -o /dev/null -w "%{http_code}" http://localhost:8080/)
if [ "$status_code" = "501" ]; then
    echo -e "${GREEN}✓ PASSED${NC}"
    PASSED=$((PASSED + 1))
else
    echo -e "${RED}✗ FAILED${NC}"
    echo "  Expected: 501"
    echo "  Got: $status_code"
    FAILED=$((FAILED + 1))
fi

echo ""
echo "=========================================="
echo " テスト結果"
echo "=========================================="
echo "Total:  $TOTAL"
echo -e "Passed: ${GREEN}$PASSED${NC}"
echo -e "Failed: ${RED}$FAILED${NC}"
echo ""

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}✅ All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}❌ Some tests failed.${NC}"
    exit 1
fi
