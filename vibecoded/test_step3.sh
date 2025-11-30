#!/bin/bash
# 簡易テストスクリプト

echo "=== Webserv Test Script ==="
echo ""

# サーバーをバックグラウンドで起動
echo "Starting webserv..."
./webserv &
SERVER_PID=$!
sleep 2

echo ""
echo "=== Testing port 8080 ==="
echo "Sending test request to localhost:8080"
echo -e "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc localhost 8080 -w 1 2>/dev/null || echo "Connection test sent"

sleep 1

echo ""
echo "=== Stopping server ==="
kill -INT $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

echo ""
echo "=== Test completed ==="
