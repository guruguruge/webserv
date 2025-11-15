#!/bin/bash

# ==========================================
# Step 9: CGI テストスクリプト
# ==========================================

HOST="localhost"
PORT="8080"

PASSED=0
FAILED=0

# サーバーが起動しているか確認
if ! nc -zv $HOST $PORT 2>&1 | grep -q succeeded; then
    echo "❌ Server is not running on $HOST:$PORT"
    echo "Please start the server with: ./webserv config/default.conf"
    exit 1
fi

echo "=========================================="
echo " Step 9: CGI テスト"
echo "=========================================="
echo

# ==========================================
# テスト関数
# ==========================================

test_result() {
    if [ $? -eq 0 ]; then
        echo "✓ PASSED"
        PASSED=$((PASSED + 1))
    else
        echo "✗ FAILED"
        FAILED=$((FAILED + 1))
    fi
}

# ==========================================
# 1. Python CGI (GET)
# ==========================================

echo "=== 1. Python CGI (GET) ==="

echo -n "Test 1: Python CGI responds with 200 OK ... "
RESPONSE=$(curl -s http://$HOST:$PORT/cgi-bin/test.py -w "\n%{http_code}")
STATUS=$(echo "$RESPONSE" | tail -n 1)
BODY=$(echo "$RESPONSE" | sed '$d')
echo "$RESPONSE" > /tmp/test1_cgi_response.txt
[ "$STATUS" = "200" ]
test_result

echo -n "Test 2: Response contains HTML ... "
echo "$BODY" | grep -q "<html>"
test_result

echo -n "Test 3: Response contains environment variables ... "
echo "$BODY" | grep -q "REQUEST_METHOD"
test_result

echo -n "Test 4: REQUEST_METHOD is GET ... "
echo "$BODY" | grep -q "REQUEST_METHOD.*GET"
test_result

echo -n "Test 5: Content-Type is text/html ... "
curl -s -i http://$HOST:$PORT/cgi-bin/test.py | grep -i "Content-Type:" | grep -q "text/html"
test_result

# ==========================================
# 2. Python CGI with Query String
# ==========================================

echo
echo "=== 2. Python CGI with Query String ==="

echo -n "Test 6: CGI with query string ... "
RESPONSE=$(curl -s "http://$HOST:$PORT/cgi-bin/test.py?name=webserv&version=1.0")
echo "$RESPONSE" | grep -q "QUERY_STRING.*name=webserv"
test_result

echo -n "Test 7: Query string is properly passed ... "
echo "$RESPONSE" | grep -q "version=1.0"
test_result

# ==========================================
# 3. Python CGI (POST)
# ==========================================

echo
echo "=== 3. Python CGI (POST) ==="

echo -n "Test 8: POST request to CGI ... "
RESPONSE=$(curl -s -X POST http://$HOST:$PORT/cgi-bin/test.py \
    -d "test=data&foo=bar")
echo "$RESPONSE" > /tmp/test8_cgi_response.txt
echo "$RESPONSE" | grep -q "REQUEST_METHOD.*POST"
test_result

echo -n "Test 9: POST data is displayed ... "
echo "$RESPONSE" | grep -q "POST Data"
test_result

echo -n "Test 10: POST data contains sent data ... "
echo "$RESPONSE" | grep -q "test=data"
test_result

# ==========================================
# 4. CGI Error Handling
# ==========================================

echo
echo "=== 4. CGI Error Handling ==="

echo -n "Test 11: Non-existent CGI script returns 404 ... "
RESPONSE=$(curl -s http://$HOST:$PORT/cgi-bin/nonexistent.py -w "\n%{http_code}")
STATUS=$(echo "$RESPONSE" | tail -n 1)
[ "$STATUS" = "404" ]
test_result

echo -n "Test 12: CGI on non-CGI path returns 200 (static file) or 404 ... "
RESPONSE=$(curl -s http://$HOST:$PORT/test.py -w "\n%{http_code}")
STATUS=$(echo "$RESPONSE" | tail -n 1)
[ "$STATUS" = "404" ] || [ "$STATUS" = "200" ]
test_result

# ==========================================
# 5. CGI Environment Variables
# ==========================================

echo
echo "=== 5. CGI Environment Variables ==="

echo -n "Test 13: SCRIPT_FILENAME is set ... "
RESPONSE=$(curl -s http://$HOST:$PORT/cgi-bin/test.py)
echo "$RESPONSE" | grep -q "SCRIPT_FILENAME"
test_result

echo -n "Test 14: SERVER_PROTOCOL is set ... "
echo "$RESPONSE" | grep -q "SERVER_PROTOCOL"
test_result

echo -n "Test 15: CONTENT_LENGTH is set for POST ... "
RESPONSE=$(curl -s -X POST http://$HOST:$PORT/cgi-bin/test.py -d "test")
echo "$RESPONSE" | grep -q "CONTENT_LENGTH.*[1-9]"
test_result

# ==========================================
# 結果サマリー
# ==========================================

echo
echo "=========================================="
echo " テスト結果"
echo "=========================================="
echo "Total:  $((PASSED + FAILED))"
echo "Passed: $PASSED"
echo "Failed: $FAILED"
echo

if [ $FAILED -eq 0 ]; then
    echo "✅ All tests passed!"
    exit 0
else
    echo "❌ Some tests failed"
    exit 1
fi
