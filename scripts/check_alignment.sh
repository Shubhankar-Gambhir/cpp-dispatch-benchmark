#!/usr/bin/env bash
set -euo pipefail

# Usage: ./scripts/check_alignment.sh <binary> [function_pattern]
# Default pattern: "store" (matches the hot dispatch functions)
# Reports function entry address, offset within 64-byte cache line,
# and offset within 32-byte DSB window.

BINARY="${1:?Usage: $0 <binary> [function_pattern]}"
PATTERN="${2:-store}"

if [ ! -f "$BINARY" ]; then
    echo "ERROR: $BINARY not found" >&2
    exit 1
fi

echo "Binary: $BINARY"
echo "Pattern: $PATTERN"
echo ""
echo "| Function | Address | CL offset (mod 64) | DSB offset (mod 32) | CL aligned? | DSB aligned? |"
echo "|----------|---------|--------------------|--------------------|-------------|--------------|"

while IFS= read -r line; do
    addr_hex=$(echo "$line" | grep -oP '^[0-9a-f]+')
    func_name=$(echo "$line" | grep -oP '<[^>]+>')
    addr_dec=$((16#$addr_hex))
    cl_offset=$((addr_dec % 64))
    dsb_offset=$((addr_dec % 32))
    [ "$cl_offset" -eq 0 ] && cl_ok="YES" || cl_ok="no ($cl_offset)"
    [ "$dsb_offset" -eq 0 ] && dsb_ok="YES" || dsb_ok="no ($dsb_offset)"
    echo "| $func_name | 0x$addr_hex | $cl_offset | $dsb_offset | $cl_ok | $dsb_ok |"
done < <(objdump -d "$BINARY" | grep -E "^[0-9a-f]+ <.*${PATTERN}.*>:" || true)
