#!/usr/bin/env bash
set -euo pipefail

# Benchmark clean build times and peak memory usage across N runs
RUNS="${1:-3}"
BUILD_DIR=".build"

if [[ ! -d "$BUILD_DIR" ]]; then
    echo "Error: $BUILD_DIR directory not found. Please configure the project first."
    exit 1
fi

echo "Running $RUNS clean build benchmark(s)..."
for i in $(seq 1 "$RUNS"); do
    echo "=== Run $i ==="
    ninja -C "$BUILD_DIR" -t clean > /dev/null
    /usr/bin/time -f "Run $i: wall=%e user=%U sys=%S maxrss=%M KB" ninja -C "$BUILD_DIR"
done
