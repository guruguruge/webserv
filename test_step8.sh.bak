#!/bin/bash

# ==========================================
# Step 8: POST/DELETE テストスクリプト
# ==========================================

HOST="localhost"
PORT="8080"

PASSED=0
FAILED=0

# テスト用の一時ファイル
TEST_FILE="test_upload.txt"
TEST_CONTENT="Hello, this is a test file for upload!"

# サーバーが起動しているか確認
if ! nc -zv $HOST $PORT 2>&1 | grep -q succeeded; then
    echo "❌ Server is not running on $HOST:$PORT"
    echo "Please start the server with: ./webserv config/default.conf"
    exit 1
fi

echo "=========================================="
echo " Step 8: POST/DELETE テスト"
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
# 1. POST アップロード (raw body)
# ==========================================

echo "=== 1. POST アップロード (raw body) ==="

echo -n "Test 1: Upload file with POST ... "
RESPONSE=$(echo "$TEST_CONTENT" | curl -s -X POST http://$HOST:$PORT/upload -d @- -w "\n%{http_code}")
STATUS=$(echo "$RESPONSE" | tail -n 1)
BODY=$(echo "$RESPONSE" | sed '$d')
echo "$RESPONSE" > /tmp/test1_response.txt
[ "$STATUS" = "201" ] && echo "$BODY" | grep -q "Upload Successful"
test_result

echo -n "Test 2: POST returns 201 Created ... "
[ "$STATUS" = "201" ]
test_result

echo -n "Test 3: Response contains filename ... "
echo "$BODY" | grep -q "upload_"
test_result

# ==========================================
# 2. POST multipart/form-data
# ==========================================

echo
echo "=== 2. POST multipart/form-data ==="

echo -n "Test 4: Upload with multipart/form-data ... "
# 一時ファイルを作成
echo "Test file content for multipart upload" > /tmp/$TEST_FILE
RESPONSE=$(curl -s -X POST http://$HOST:$PORT/upload \
    -F "file=@/tmp/$TEST_FILE" \
    -w "\n%{http_code}")
STATUS=$(echo "$RESPONSE" | tail -n 1)
BODY=$(echo "$RESPONSE" | sed '$d')
echo "$RESPONSE" > /tmp/test4_response.txt
[ "$STATUS" = "201" ] && echo "$BODY" | grep -q "Upload Successful"
test_result

echo -n "Test 5: Multipart response contains filename ... "
echo "$BODY" | grep -q "$TEST_FILE"
test_result

# ==========================================
# 3. DELETE メソッド
# ==========================================

echo
echo "=== 3. DELETE メソッド ==="

# テスト用ファイルを作成
TEST_DELETE_FILE="www/uploads/test_delete.txt"
echo "This file will be deleted" > $TEST_DELETE_FILE

echo -n "Test 6: DELETE existing file ... "
RESPONSE=$(curl -s -X DELETE http://$HOST:$PORT/upload/test_delete.txt -w "\n%{http_code}")
STATUS=$(echo "$RESPONSE" | tail -n 1)
echo "$RESPONSE" > /tmp/test6_response.txt
[ "$STATUS" = "204" ]
test_result

echo -n "Test 7: File is actually deleted ... "
[ ! -f "$TEST_DELETE_FILE" ]
test_result

echo -n "Test 8: DELETE non-existent file returns 404 ... "
RESPONSE=$(curl -s -X DELETE http://$HOST:$PORT/upload/nonexistent.txt -w "\n%{http_code}")
STATUS=$(echo "$RESPONSE" | tail -n 1)
echo "$RESPONSE" > /tmp/test8_response.txt
[ "$STATUS" = "404" ]
test_result

# ==========================================
# 4. エラーケース
# ==========================================

echo
echo "=== 4. エラーケース ==="

echo -n "Test 9: POST to path without upload_path returns 403 ... "
RESPONSE=$(curl -s -X POST http://$HOST:$PORT/ -d "test" -w "\n%{http_code}")
STATUS=$(echo "$RESPONSE" | tail -n 1)
echo "$RESPONSE" > /tmp/test9_response.txt
[ "$STATUS" = "403" ]
test_result

echo -n "Test 10: DELETE on path without DELETE method returns 405 ... "
RESPONSE=$(curl -s -X DELETE http://$HOST:$PORT/ -w "\n%{http_code}")
STATUS=$(echo "$RESPONSE" | tail -n 1)
echo "$RESPONSE" > /tmp/test10_response.txt
[ "$STATUS" = "405" ]
test_result

echo -n "Test 11: DELETE directory returns 403 ... "
mkdir -p www/uploads/testdir
RESPONSE=$(curl -s -X DELETE http://$HOST:$PORT/upload/testdir -w "\n%{http_code}")
STATUS=$(echo "$RESPONSE" | tail -n 1)
echo "$RESPONSE" > /tmp/test11_response.txt
rmdir www/uploads/testdir 2>/dev/null
[ "$STATUS" = "403" ]
test_result

# ==========================================
# 5. Content-Type チェック
# ==========================================

echo
echo "=== 5. Content-Type チェック ==="

echo -n "Test 12: Upload response has Content-Type: text/html ... "
RESPONSE=$(echo "test" | curl -s -X POST http://$HOST:$PORT/upload -d @- -i)
echo "$RESPONSE" | grep -i "Content-Type:" | grep -q "text/html"
test_result

echo -n "Test 13: DELETE response has no body (204) ... "
echo "Delete me" > www/uploads/test_delete2.txt
RESPONSE=$(curl -s -X DELETE http://$HOST:$PORT/upload/test_delete2.txt -w "\n%{http_code}")
BODY=$(echo "$RESPONSE" | sed '$d')
STATUS=$(echo "$RESPONSE" | tail -n 1)
[ "$STATUS" = "204" ] && [ -z "$BODY" ]
test_result

# ==========================================
# 6. アップロードされたファイルの確認
# ==========================================

echo
echo "=== 6. アップロードされたファイルの確認 ==="

echo -n "Test 14: Uploaded files exist in uploads directory ... "
FILE_COUNT=$(ls www/uploads/upload_* 2>/dev/null | wc -l)
[ $FILE_COUNT -gt 0 ]
test_result

echo -n "Test 15: Can GET uploaded file through autoindex ... "
RESPONSE=$(curl -s http://$HOST:$PORT/upload/)
echo "$RESPONSE" | grep -q "upload_"
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
