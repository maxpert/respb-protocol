#!/bin/bash
cd /Users/zohaib/repos/respb-valkey/protocol-bench

echo "Building tests..."
make clean > /dev/null 2>&1
make bin/test > /dev/null 2>&1

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

echo "Running tests..."
./bin/test 2>&1 | tee /tmp/test_final.txt

EXIT_CODE=$?

echo ""
echo "=== Results ==="
grep -E "Total Tests|Passed|Failed" /tmp/test_final.txt | tail -3

if grep -q "FAIL" /tmp/test_final.txt; then
    echo ""
    echo "=== Failures ==="
    grep -B 1 "FAIL" /tmp/test_final.txt
    exit 1
fi

if [ $EXIT_CODE -eq 0 ]; then
    echo ""
    echo "✓ All tests passed!"
    exit 0
else
    exit $EXIT_CODE
fi

