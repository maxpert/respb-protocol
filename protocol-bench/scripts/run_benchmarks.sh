#!/bin/bash
#
# Run comprehensive protocol benchmarks
# Compares RESP and RESPB performance across different workload types

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BENCH_DIR="$(dirname "$SCRIPT_DIR")"
BENCHMARK="$BENCH_DIR/bin/benchmark"
DATA_DIR="$BENCH_DIR/data"
RESULTS_DIR="$BENCH_DIR/results"

# Create directories
mkdir -p "$RESULTS_DIR"

# Check if benchmark binary exists
if [[ ! -f "$BENCHMARK" ]]; then
    echo "Error: Benchmark binary not found at $BENCHMARK"
    echo "Run 'make' to build it first."
    exit 1
fi

# Check if workload files exist
if [[ ! -f "$DATA_DIR/workload_mixed_resp.bin" ]]; then
    echo "Workload files not found. Generating..."
    cd "$BENCH_DIR"
    python3 scripts/generate_workloads.py --output data --size 10
fi

echo "=========================================="
echo "  Protocol Benchmark Suite"
echo "=========================================="
echo ""

ITERATIONS=100
WORKLOAD_TYPES="small medium large mixed"

for TYPE in $WORKLOAD_TYPES; do
    echo "=========================================="
    echo "  Workload: $TYPE"
    echo "=========================================="
    echo ""
    
    RESP_FILE="$DATA_DIR/workload_${TYPE}_resp.bin"
    RESPB_FILE="$DATA_DIR/workload_${TYPE}_respb.bin"
    
    if [[ ! -f "$RESP_FILE" ]] || [[ ! -f "$RESPB_FILE" ]]; then
        echo "Warning: Workload files for '$TYPE' not found, skipping..."
        continue
    fi
    
    echo "--- Running RESP benchmark ---"
    "$BENCHMARK" -r "$RESP_FILE" -p resp -i $ITERATIONS -l > "$RESULTS_DIR/${TYPE}_resp.txt" 2>&1
    cat "$RESULTS_DIR/${TYPE}_resp.txt"
    
    echo "--- Running RESPB benchmark ---"
    "$BENCHMARK" -b "$RESPB_FILE" -p respb -i $ITERATIONS -l > "$RESULTS_DIR/${TYPE}_respb.txt" 2>&1
    cat "$RESULTS_DIR/${TYPE}_respb.txt"
    
    echo ""
done

echo "=========================================="
echo "  Benchmarks Complete!"
echo "=========================================="
echo ""
echo "Results saved in: $RESULTS_DIR/"
echo ""
echo "Summary:"
for TYPE in $WORKLOAD_TYPES; do
    RESP_FILE="$RESULTS_DIR/${TYPE}_resp.txt"
    RESPB_FILE="$RESULTS_DIR/${TYPE}_respb.txt"
    
    if [[ -f "$RESP_FILE" ]] && [[ -f "$RESPB_FILE" ]]; then
        RESP_THROUGHPUT=$(grep "Throughput:" "$RESP_FILE" | awk '{print $2}')
        RESPB_THROUGHPUT=$(grep "Throughput:" "$RESPB_FILE" | awk '{print $2}')
        
        echo "  $TYPE: RESP=$RESP_THROUGHPUT cmd/s, RESPB=$RESPB_THROUGHPUT cmd/s"
    fi
done
echo ""

