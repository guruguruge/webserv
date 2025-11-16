#!/bin/bash

# ==========================================
# 全テストスイート実行スクリプト
# ==========================================

echo "=========================================="
echo " webserv - 全テストスイート"
echo "=========================================="
echo

# サーバーが起動しているか確認
if ! nc -zv localhost 8080 2>&1 | grep -q succeeded; then
    echo "❌ Server is not running on localhost:8080"
    echo "Please start the server with: ./webserv config/default.conf"
    exit 1
fi

TOTAL_PASSED=0
TOTAL_FAILED=0

# ==========================================
# テスト実行関数
# ==========================================

run_test() {
    local test_name=$1
    local test_script=$2
    
    echo
    echo "=========================================="
    echo " Running: $test_name"
    echo "=========================================="
    
    if [ ! -f "$test_script" ]; then
        echo "⚠️  Test script not found: $test_script"
        return
    fi
    
    chmod +x "$test_script"
    
    if ./"$test_script"; then
        echo "✅ $test_name: PASSED"
    else
        echo "❌ $test_name: FAILED"
        TOTAL_FAILED=$((TOTAL_FAILED + 1))
        return 1
    fi
    
    TOTAL_PASSED=$((TOTAL_PASSED + 1))
}

# ==========================================
# テスト実行
# ==========================================

# Step 8: POST/DELETE
if [ -f "test_step8.sh" ]; then
    run_test "Step 8: POST/DELETE" "test_step8.sh"
fi

# Step 9: CGI
if [ -f "test_step9.sh" ]; then
    run_test "Step 9: CGI" "test_step9.sh"
fi

# Step 10: keep-alive & timeout
if [ -f "test_step10.sh" ]; then
    run_test "Step 10: keep-alive & timeout" "test_step10.sh"
fi

# Step 11: Stress test
if [ -f "test_stress.sh" ]; then
    run_test "Step 11: Stress test" "test_stress.sh"
fi

# ==========================================
# 最終結果
# ==========================================

echo
echo "=========================================="
echo " 全テスト結果サマリー"
echo "=========================================="
echo "Test Suites Passed: $TOTAL_PASSED"
echo "Test Suites Failed: $TOTAL_FAILED"
echo

if [ $TOTAL_FAILED -eq 0 ]; then
    echo "🎉 All test suites passed!"
    exit 0
else
    echo "❌ Some test suites failed"
    exit 1
fi
