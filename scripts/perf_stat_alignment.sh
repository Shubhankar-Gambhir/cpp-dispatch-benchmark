#!/usr/bin/env bash
set -euo pipefail

# Run on Intel Xeon Gold 6130 ONLY (Intel PMU counters).
# Usage: ./scripts/perf_stat_alignment.sh
# Requires: perf with PMU access (perf_event_paranoid <= 1 or root)
# Run 'make align' first to build binaries.

REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_DIR"

RESULTS_DIR="$REPO_DIR/results"
mkdir -p "$RESULTS_DIR"

GC_ARG="g1"

# The three specimens from the spec
declare -A SPECIMENS=(
    ["virtual_gcc13_default"]="build/bench_virtual_gcc13_default"
    ["virtual_gcc13_align64"]="build/bench_virtual_gcc13_align64"
    ["virtual_gcc11_default"]="build/bench_virtual_gcc11_default"
)

# Intel Skylake frontend counters
# Group 1: DSB vs MITE delivery
# Group 2: Instruction cache
# Group 3: ITLB
COUNTERS="idq.dsb_uops,idq.mite_uops,frontend_retired.dsb_miss,L1-icache-load-misses,itlb_misses.miss_causes_a_walk"

echo "| Specimen | DSB uops | MITE uops | DSB miss | L1i miss | ITLB walk | ns/call |"
echo "|----------|----------|-----------|----------|----------|-----------|---------|"

for name in virtual_gcc13_default virtual_gcc13_align64 virtual_gcc11_default; do
    binary="${SPECIMENS[$name]}"
    if [ ! -f "$binary" ]; then
        echo "ERROR: $binary not found. Run 'make align' first." >&2
        exit 1
    fi

    # Capture perf stat output (stderr) to file
    perf_out="$RESULTS_DIR/perf_${name}.txt"
    taskset -c 0 perf stat -e "$COUNTERS" "./$binary" "$GC_ARG" 2>"$perf_out"

    # Parse ns/call from benchmark stdout
    ns=$(taskset -c 0 "./$binary" "$GC_ARG" 2>/dev/null | grep -oP '[\d.]+(?= ns/call)')
    [ -z "$ns" ] && { echo "ERROR: $binary produced no timing output" >&2; exit 1; }

    # Parse counters from perf output (perf stat writes to stderr, redirected to file)
    extract() { grep "$1" "$perf_out" | awk '{gsub(",","",$1); print $1}'; }
    dsb_uops=$(extract "idq.dsb_uops")
    mite_uops=$(extract "idq.mite_uops")
    dsb_miss=$(extract "frontend_retired.dsb_miss")
    l1i_miss=$(extract "L1-icache-load-misses")
    itlb_walk=$(extract "itlb_misses")

    echo "| $name | ${dsb_uops:-N/A} | ${mite_uops:-N/A} | ${dsb_miss:-N/A} | ${l1i_miss:-N/A} | ${itlb_walk:-N/A} | $ns |"
done

echo ""
echo "Raw perf output saved to $RESULTS_DIR/perf_*.txt"
