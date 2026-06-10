#!/usr/bin/env bash
set -euo pipefail

# Run from the companion repo root: ./scripts/run_alignment_benchmarks.sh
# Requires: GCC 11/13/15 in ~/utils/mamba/envs/, taskset available
# Output: markdown table to stdout, raw data to results/alignment_raw.csv

REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_DIR"

RESULTS_DIR="$REPO_DIR/results"
mkdir -p "$RESULTS_DIR"

MECHANISMS=(bench_virtual bench_fnptr bench_variant bench_crtp)
GCCS=(gcc11 gcc13 gcc15)
ALIGNS=(default align32 align64)
RUNS=3
GC_ARG="g1"
RAW_CSV="$RESULTS_DIR/alignment_raw.csv"

echo "mechanism,gcc,alignment,run,ns_per_call" > "$RAW_CSV"

# Build all binaries first
echo "Building 36 binaries..."
make align

echo ""
echo "Running benchmarks (best of $RUNS runs each)..."
echo ""

# Header for markdown table
echo "| Mechanism | GCC | Default | align=32 | align=64 |"
echo "|-----------|-----|---------|----------|----------|"

for mech in "${MECHANISMS[@]}"; do
    for gcc in "${GCCS[@]}"; do
        row="| ${mech#bench_} | ${gcc#gcc} |"
        for align in "${ALIGNS[@]}"; do
            binary="build/${mech}_${gcc}_${align}"
            best=""
            for run in $(seq 1 $RUNS); do
                ns=$(taskset -c 0 "./$binary" "$GC_ARG" 2>&1 | grep -oP '[\d.]+(?= ns/call)')
                [ -z "$ns" ] && { echo "ERROR: $binary produced no output or failed" >&2; exit 1; }
                echo "${mech},${gcc},${align},${run},${ns}" >> "$RAW_CSV"
                if [ -z "$best" ] || (( $(echo "$ns < $best" | bc -l) )); then
                    best="$ns"
                fi
            done
            row="$row $best |"
        done
        echo "$row"
    done
done

echo ""
echo "Raw data saved to $RAW_CSV"
