#!/bin/bash
# RESPB Protocol Benchmark Runner
# Builds, tests, runs benchmarks, and analyzes results

set -e

cd "$(dirname "$0")/protocol-bench"

echo "=========================================="
echo "  RESPB Protocol Benchmark"
echo "=========================================="
echo ""

# Step 1: Build
echo "[1/5] Building benchmark..."
make clean > /dev/null 2>&1 || true
make 2>&1 | tail -5
if [ -f "bin/benchmark" ]; then
    echo "✓ Build successful"
else
    echo "✗ Build failed"
    exit 1
fi
echo ""

# Step 2: Run tests
echo "[2/5] Running tests..."
make test
echo ""

# Step 3: Ensure workloads exist
echo "[3/5] Checking workloads..."
if [ ! -f "data/workload_mixed_resp.bin" ]; then
    echo "Generating workloads..."
    python3 scripts/generate_workloads.py --output data --size 10
fi
echo "✓ Workloads ready"
echo ""

# Step 4: Run benchmarks
echo "[4/5] Running benchmarks..."
mkdir -p results
bash scripts/run_benchmarks.sh
echo ""

# Step 5: Analyze results
echo "[5/5] Analyzing results..."
python3 scripts/analyze_results.py results/
echo ""

echo "=========================================="
echo "  Benchmark Complete!"
echo "=========================================="
echo ""
echo "Results: protocol-bench/results/"
echo ""
