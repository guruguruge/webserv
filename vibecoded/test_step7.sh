#!/bin/bash

# ==================================================
# Step 7 テストスクリプト: エラーページ管理の動作確認
# ==================================================

echo "=========================================="
echo " Step 7: Error Page Management テスト"
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

echo "=== 1. カスタムエラーページ ==="
test_request "Custom 404 page" "The requested resource could not be found" \
    curl -s http://localhost:8080/nonexistent.html

test_request "Custom 404 has correct status" "HTTP/1.1 404" \
    curl -I -s http://localhost:8080/nonexistent.html

# カスタムエラーページのタイトルもチェック
test_request "Custom 404 page title" "404 Not Found" \
    curl -s http://localhost:8080/nonexistent.html

echo ""
echo "=== 2. デフォルトエラーページ ==="
test_request "Default 501 page" "Not Implemented" \
    curl -s -X POST http://localhost:8080/test

test_request "Default 501 has styled container" "error-container" \
    curl -s -X POST http://localhost:8080/test

test_request "Default 501 has webserv signature" "webserv/1.0" \
    curl -s -X POST http://localhost:8080/test

test_request "Default 403 page" "403" \
    curl -s http://localhost:8080/noindex/

echo ""
echo "=== 3. 各種エラーステータス ==="
test_request "501 Not Implemented (PUT not supported)" "501" \
    curl -s -X PUT http://localhost:8080/

test_request "400 Bad Request" "400" \
    sh -c 'printf "INVALID HTTP\r\n\r\n" | nc localhost 8080'

echo ""
echo "=== 4. エラーページのContent-Type ==="
test_request "Error page is HTML" "Content-Type: text/html" \
    curl -I -s http://localhost:8080/notfound

test_request "Error page has Content-Length" "Content-Length:" \
    curl -I -s http://localhost:8080/notfound

echo ""
echo "=== 5. 正常なレスポンスも動作確認 ==="
test_request "Normal 200 response still works" "Welcome to Webserv" \
    curl -s http://localhost:8080/

test_request "Normal 200 has correct status" "HTTP/1.1 200" \
    curl -I -s http://localhost:8080/

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
