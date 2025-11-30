#!/bin/bash

# ==========================================
# Step 10: keep-alive とタイムアウト テストスクリプト
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
echo " Step 10: keep-alive とタイムアウト テスト"
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
# 1. HTTP/1.1 keep-alive (デフォルト)
# ==========================================

echo "=== 1. HTTP/1.1 keep-alive (デフォルト) ==="

echo -n "Test 1: HTTP/1.1 request without Connection header defaults to keep-alive ... "
RESPONSE=$(curl -s -i http://$HOST:$PORT/)
echo "$RESPONSE" | grep -qi "Connection: keep-alive"
test_result

echo -n "Test 2: Multiple requests on same connection (keep-alive) ... "
# 同じ接続で2つのリクエストを送信
{
    echo -ne "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
    sleep 0.5
    echo -ne "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
} | nc $HOST $PORT > /tmp/test2_keepalive.txt 2>&1

# 2つのレスポンスが返ってくることを確認
COUNT=$(grep -c "HTTP/1.1" /tmp/test2_keepalive.txt)
[ "$COUNT" -ge 2 ]
test_result

# ==========================================
# 2. HTTP/1.1 Connection: close
# ==========================================

echo
echo "=== 2. HTTP/1.1 Connection: close ==="

echo -n "Test 3: HTTP/1.1 with Connection: close header ... "
RESPONSE=$(curl -s -i -H "Connection: close" http://$HOST:$PORT/)
echo "$RESPONSE" | grep -qi "Connection: close"
test_result

echo -n "Test 4: Connection closes after response when Connection: close ... "
# Connection: close の場合、接続が閉じられることを確認
{
    echo -ne "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
    sleep 0.5
    echo -ne "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
} | nc $HOST $PORT > /tmp/test4_close.txt 2>&1

# 1つのレスポンスだけが返ってくることを確認（接続が閉じられるため）
COUNT=$(grep -c "HTTP/1.1" /tmp/test4_close.txt)
[ "$COUNT" -eq 1 ]
test_result

# ==========================================
# 3. HTTP/1.0 (デフォルトは close)
# ==========================================

echo
echo "=== 3. HTTP/1.0 (デフォルトは close) ==="

echo -n "Test 5: HTTP/1.0 request defaults to Connection: close ... "
{
    echo -ne "GET / HTTP/1.0\r\nHost: localhost\r\n\r\n"
} | nc $HOST $PORT > /tmp/test5_http10.txt 2>&1

grep -qi "Connection: close" /tmp/test5_http10.txt
test_result

echo -n "Test 6: HTTP/1.0 with Connection: keep-alive ... "
{
    echo -ne "GET / HTTP/1.0\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n"
} | nc $HOST $PORT > /tmp/test6_http10_keepalive.txt 2>&1

grep -qi "Connection: keep-alive" /tmp/test6_http10_keepalive.txt
test_result

# ==========================================
# 4. 複数リクエストの連続送信
# ==========================================

echo
echo "=== 4. 複数リクエストの連続送信 ==="

echo -n "Test 7: Three consecutive requests on keep-alive connection ... "
{
    echo -ne "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
    sleep 0.3
    echo -ne "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n"
    sleep 0.3
    echo -ne "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
} | nc $HOST $PORT > /tmp/test7_multiple.txt 2>&1

COUNT=$(grep -c "HTTP/1.1" /tmp/test7_multiple.txt)
[ "$COUNT" -ge 3 ]
test_result

# ==========================================
# 5. 異なるリソースへのリクエスト
# ==========================================

echo
echo "=== 5. 異なるリソースへのリクエスト (keep-alive) ==="

echo -n "Test 8: Request to different paths on same connection ... "
{
    echo -ne "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
    sleep 0.3
    echo -ne "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n"
} | nc $HOST $PORT > /tmp/test8_different_paths.txt 2>&1

COUNT=$(grep -c "HTTP/1.1" /tmp/test8_different_paths.txt)
[ "$COUNT" -ge 2 ]
test_result

# ==========================================
# 6. タイムアウトのテスト（簡易版）
# ==========================================

echo
echo "=== 6. タイムアウト ==="

echo -n "Test 9: Connection stays alive within timeout period ... "
# 短時間の間隔でリクエストを送信（タイムアウトしないことを確認）
{
    echo -ne "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
    sleep 2
    echo -ne "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
} | nc $HOST $PORT > /tmp/test9_timeout.txt 2>&1

COUNT=$(grep -c "HTTP/1.1" /tmp/test9_timeout.txt)
[ "$COUNT" -ge 2 ]
test_result

echo -n "Test 10: Server responds correctly after keep-alive ... "
# keep-alive後もサーバーが正常に動作することを確認
RESPONSE=$(curl -s http://$HOST:$PORT/ -w "\n%{http_code}")
STATUS=$(echo "$RESPONSE" | tail -n 1)
[ "$STATUS" = "200" ]
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
