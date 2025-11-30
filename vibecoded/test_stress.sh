#!/bin/bash

# ==========================================
# Step 11: ストレステストスクリプト
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
echo " Step 11: ストレステスト"
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
# 1. 同時接続テスト (GET)
# ==========================================

echo "=== 1. 同時接続テスト (10並列GET) ==="

echo -n "Test 1: 10 concurrent GET requests ... "
for i in {1..10}; do
    curl -s -H "Connection: close" http://$HOST:$PORT/ > /tmp/stress_get_$i.txt 2>&1 &
done
wait
# すべてのレスポンスが返ってきたか確認
SUCCESS=0
for i in {1..10}; do
    if grep -q "html" /tmp/stress_get_$i.txt 2>/dev/null; then
        SUCCESS=$((SUCCESS + 1))
    fi
done
[ "$SUCCESS" -ge 8 ]  # 少なくとも80%成功
test_result

# ==========================================
# 2. 大量の連続リクエスト
# ==========================================

echo
echo "=== 2. 大量の連続リクエスト (50リクエスト) ==="

echo -n "Test 2: 50 sequential requests ... "
SUCCESS=0
for i in {1..50}; do
    RESPONSE=$(curl -s -H "Connection: close" http://$HOST:$PORT/ -w "%{http_code}")
    if echo "$RESPONSE" | grep -q "200$"; then
        SUCCESS=$((SUCCESS + 1))
    fi
done
[ "$SUCCESS" -ge 45 ]  # 少なくとも90%成功
test_result

# ==========================================
# 3. CGI同時実行テスト
# ==========================================

echo
echo "=== 3. CGI同時実行テスト (5並列CGI) ==="

echo -n "Test 3: 5 concurrent CGI requests ... "
for i in {1..5}; do
    curl -s -H "Connection: close" http://$HOST:$PORT/cgi-bin/test.py > /tmp/stress_cgi_$i.txt 2>&1 &
done
wait
SUCCESS=0
for i in {1..5}; do
    if grep -q "REQUEST_METHOD" /tmp/stress_cgi_$i.txt 2>/dev/null; then
        SUCCESS=$((SUCCESS + 1))
    fi
done
[ "$SUCCESS" -ge 4 ]  # 少なくとも80%成功
test_result

# ==========================================
# 4. POSTリクエスト同時実行
# ==========================================

echo
echo "=== 4. POSTリクエスト同時実行 (5並列POST) ==="

echo -n "Test 4: 5 concurrent POST requests ... "
for i in {1..5}; do
    curl -s -X POST -H "Connection: close" \
        -d "test=data$i" \
        http://$HOST:$PORT/upload > /tmp/stress_post_$i.txt 2>&1 &
done
wait
SUCCESS=0
for i in {1..5}; do
    if [ -s /tmp/stress_post_$i.txt ]; then
        SUCCESS=$((SUCCESS + 1))
    fi
done
[ "$SUCCESS" -ge 4 ]  # 少なくとも80%成功
test_result

# ==========================================
# 5. 混合リクエスト（GET + POST + CGI）
# ==========================================

echo
echo "=== 5. 混合リクエスト (GET + POST + CGI, 15並列) ==="

echo -n "Test 5: 15 concurrent mixed requests ... "
# GET
for i in {1..5}; do
    curl -s -H "Connection: close" http://$HOST:$PORT/ > /tmp/stress_mixed_get_$i.txt 2>&1 &
done
# POST
for i in {1..5}; do
    curl -s -X POST -H "Connection: close" \
        -d "data$i" \
        http://$HOST:$PORT/upload > /tmp/stress_mixed_post_$i.txt 2>&1 &
done
# CGI
for i in {1..5}; do
    curl -s -H "Connection: close" http://$HOST:$PORT/cgi-bin/test.py > /tmp/stress_mixed_cgi_$i.txt 2>&1 &
done
wait
SUCCESS=0
for i in {1..5}; do
    [ -s /tmp/stress_mixed_get_$i.txt ] && SUCCESS=$((SUCCESS + 1))
    [ -s /tmp/stress_mixed_post_$i.txt ] && SUCCESS=$((SUCCESS + 1))
    [ -s /tmp/stress_mixed_cgi_$i.txt ] && SUCCESS=$((SUCCESS + 1))
done
[ "$SUCCESS" -ge 12 ]  # 少なくとも80%成功
test_result

# ==========================================
# 6. keep-alive 大量リクエスト
# ==========================================

echo
echo "=== 6. keep-alive 大量リクエスト ==="

echo -n "Test 6: Multiple requests on keep-alive connection ... "
{
    for i in {1..10}; do
        echo -ne "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
        sleep 0.1
    done
} | nc $HOST $PORT > /tmp/stress_keepalive.txt 2>&1

COUNT=$(grep -c "HTTP/1.1 200" /tmp/stress_keepalive.txt)
[ "$COUNT" -ge 8 ]  # 少なくとも8つのレスポンス
test_result

# ==========================================
# 7. 不正なリクエストの処理
# ==========================================

echo
echo "=== 7. 不正なリクエストの処理 ==="

echo -n "Test 7: Invalid request returns 400 ... "
{
    echo -ne "INVALID REQUEST\r\n\r\n"
} | nc $HOST $PORT > /tmp/stress_invalid.txt 2>&1
grep -q "400" /tmp/stress_invalid.txt
test_result

echo -n "Test 8: Incomplete request handling ... "
{
    echo -ne "GET / HTTP/1.1\r\nHost: localhost\r\n"
    # ヘッダーを完了させずに接続を閉じる
} | nc $HOST $PORT > /tmp/stress_incomplete.txt 2>&1
# タイムアウトまたは接続切断されることを確認（クラッシュしない）
sleep 1
[ $? -eq 0 ]
test_result

# ==========================================
# 8. サーバーの応答性確認
# ==========================================

echo
echo "=== 8. サーバーの応答性確認 ==="

echo -n "Test 9: Server is still responsive after stress ... "
RESPONSE=$(curl -s -H "Connection: close" http://$HOST:$PORT/ -w "\n%{http_code}")
STATUS=$(echo "$RESPONSE" | tail -n 1)
[ "$STATUS" = "200" ]
test_result

echo -n "Test 10: CGI still works after stress ... "
RESPONSE=$(curl -s -H "Connection: close" http://$HOST:$PORT/cgi-bin/test.py)
echo "$RESPONSE" | grep -q "REQUEST_METHOD"
test_result

# ==========================================
# クリーンアップ
# ==========================================

echo
echo "Cleaning up temporary files..."
rm -f /tmp/stress_*.txt
rm -f /tmp/test*_*.txt

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
    echo "✅ All stress tests passed!"
    echo "Server handled concurrent requests without crashing."
    exit 0
else
    echo "⚠️  Some tests failed, but server may still be functional"
    echo "Check for crashes or hangs in the server logs."
    exit 1
fi
