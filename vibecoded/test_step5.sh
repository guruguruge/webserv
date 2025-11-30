#!/bin/bash

# ==================================================
# Step 5 テストスクリプト: HttpRequestParser の動作確認
# ==================================================

echo "=========================================="
echo " Step 5: HTTP Request Parser テスト"
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

echo "=== 1. 基本的な GET リクエスト ==="
test_request "Simple GET request" "Request Parsed" \
    curl -s "http://localhost:8080/"

test_request "GET with path" "Path: /test" \
    curl -s "http://localhost:8080/test"

test_request "GET with query string" "Path: /search" \
    curl -s "http://localhost:8080/search?q=webserv&page=1"

echo ""
echo "=== 2. POST リクエスト ==="
test_request "POST with Content-Length" "Request Parsed" \
    curl -s -X POST -H "Content-Type: text/plain" -d "Hello, World!" "http://localhost:8080/api"

test_request "POST with JSON" "Request Parsed" \
    curl -s -X POST -H "Content-Type: application/json" -d '{"name":"test","value":123}' "http://localhost:8080/api/data"

echo ""
echo "=== 3. チャンク転送エンコーディング ==="
# チャンク転送のテスト（ncを使用）
TOTAL=$((TOTAL + 1))
echo -n "Test $TOTAL: Chunked transfer encoding ... "
chunked_output=$(printf "POST /upload HTTP/1.1\r\nHost: localhost:8080\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n6\r\n World\r\n0\r\n\r\n" | nc localhost 8080 2>&1)
if echo "$chunked_output" | grep -q "Request Parsed"; then
    echo -e "${GREEN}✓ PASSED${NC}"
    PASSED=$((PASSED + 1))
else
    echo -e "${RED}✗ FAILED${NC}"
    FAILED=$((FAILED + 1))
fi

echo ""
echo "=== 4. エラーハンドリング ==="
TOTAL=$((TOTAL + 1))
echo -n "Test $TOTAL: Malformed request line ... "
error_output=$(printf "INVALID REQUEST\r\n\r\n" | nc localhost 8080 2>&1)
if echo "$error_output" | grep -q "400 Bad Request"; then
    echo -e "${GREEN}✓ PASSED${NC}"
    PASSED=$((PASSED + 1))
else
    echo -e "${RED}✗ FAILED${NC}"
    FAILED=$((FAILED + 1))
fi

TOTAL=$((TOTAL + 1))
echo -n "Test $TOTAL: Missing HTTP version ... "
error_output2=$(printf "GET /test\r\n\r\n" | nc localhost 8080 2>&1)
if echo "$error_output2" | grep -q "400 Bad Request"; then
    echo -e "${GREEN}✓ PASSED${NC}"
    PASSED=$((PASSED + 1))
else
    echo -e "${RED}✗ FAILED${NC}"
    FAILED=$((FAILED + 1))
fi

echo ""
echo "=== 5. ヘッダーパース ==="
test_request "Multiple headers" "Request Parsed" \
    curl -s -H "User-Agent: test-agent" -H "X-Custom-Header: custom-value" "http://localhost:8080/headers"

echo ""
echo "=== 6. 特殊なパス ==="
test_request "Root path" "Path: /" \
    curl -s "http://localhost:8080/"

test_request "Path with multiple segments" "Path: /api/v1/users/123" \
    curl -s "http://localhost:8080/api/v1/users/123"

test_request "Path with special characters" "Path: /test%20file" \
    curl -s "http://localhost:8080/test%20file"

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
