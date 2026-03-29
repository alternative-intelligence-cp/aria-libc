#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────
# benchmark.sh — Compare static (musl) vs dynamic (glibc) builds
# ─────────────────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_STATIC="$SCRIPT_DIR/build_static.sh"
ARIAC="${ARIAC:-$(realpath "$PROJECT_DIR/../aria/build/ariac" 2>/dev/null)}"

cd "$PROJECT_DIR"

OUTDIR="build/benchmarks"
mkdir -p "$OUTDIR"

# ── Benchmark targets ────────────────────────────────────────────────
declare -a TARGETS=(
    "io:tests/test_libc_io.aria:-laria_libc_io"
    "mem:tests/test_libc_mem.aria:-laria_libc_mem"
    "math:tests/test_libc_math.aria:-laria_libc_math"
    "process:tests/test_libc_process.aria:-laria_libc_process"
)

echo "═══════════════════════════════════════════════════════════════"
echo "  aria-libc Benchmark: Static (musl) vs Dynamic (glibc)"
echo "═══════════════════════════════════════════════════════════════"
echo ""

printf "  %-12s %12s %12s %12s %12s\n" "Module" "Static(KB)" "Dynamic(KB)" "Static(ms)" "Dynamic(ms)"
printf "  %-12s %12s %12s %12s %12s\n" "──────" "──────────" "───────────" "──────────" "───────────"

TOTAL_STATIC_SIZE=0
TOTAL_DYNAMIC_SIZE=0

for entry in "${TARGETS[@]}"; do
    IFS=: read -r name aria_file link_flags <<< "$entry"
    
    STATIC_BIN="$OUTDIR/${name}_static"
    DYNAMIC_BIN="$OUTDIR/${name}_dynamic"
    
    # Build static
    "$BUILD_STATIC" "$aria_file" -o "$STATIC_BIN" -Lshim $link_flags 2>/dev/null
    
    # Build dynamic (via ariac directly)
    "$ARIAC" "$aria_file" -o "$DYNAMIC_BIN" -L./shim $link_flags 2>/dev/null
    
    # Sizes
    STATIC_SIZE=$(stat -c%s "$STATIC_BIN")
    DYNAMIC_SIZE=$(stat -c%s "$DYNAMIC_BIN")
    STATIC_KB=$((STATIC_SIZE / 1024))
    DYNAMIC_KB=$((DYNAMIC_SIZE / 1024))
    TOTAL_STATIC_SIZE=$((TOTAL_STATIC_SIZE + STATIC_SIZE))
    TOTAL_DYNAMIC_SIZE=$((TOTAL_DYNAMIC_SIZE + DYNAMIC_SIZE))
    
    # Startup time (average of 10 runs)
    STATIC_TIME=0
    DYNAMIC_TIME=0
    RUNS=10
    
    for i in $(seq 1 $RUNS); do
        # Static: time in nanoseconds
        START=$(date +%s%N)
        "$STATIC_BIN" > /dev/null 2>&1 || true
        END=$(date +%s%N)
        STATIC_TIME=$((STATIC_TIME + END - START))
        
        # Dynamic
        START=$(date +%s%N)
        LD_LIBRARY_PATH="$PROJECT_DIR/shim:${LD_LIBRARY_PATH:-}" "$DYNAMIC_BIN" > /dev/null 2>&1 || true
        END=$(date +%s%N)
        DYNAMIC_TIME=$((DYNAMIC_TIME + END - START))
    done
    
    STATIC_MS=$(echo "scale=1; $STATIC_TIME / $RUNS / 1000000" | bc)
    DYNAMIC_MS=$(echo "scale=1; $DYNAMIC_TIME / $RUNS / 1000000" | bc)
    
    printf "  %-12s %10dKB %10dKB %10sms %10sms\n" \
        "$name" "$STATIC_KB" "$DYNAMIC_KB" "$STATIC_MS" "$DYNAMIC_MS"
done

echo ""
echo "  ─────────────────────────────────────────────────────────────"
TOTAL_STATIC_KB=$((TOTAL_STATIC_SIZE / 1024))
TOTAL_DYNAMIC_KB=$((TOTAL_DYNAMIC_SIZE / 1024))
printf "  %-12s %10dKB %10dKB\n" "TOTAL" "$TOTAL_STATIC_KB" "$TOTAL_DYNAMIC_KB"
echo ""

# Portability check
echo "  Portability:"
echo "  Static binaries have ZERO runtime dependencies."
echo "  Dynamic binaries require:"
set +o pipefail
DEPS=$(ldd "$OUTDIR/io_dynamic" 2>/dev/null | grep "=>" | awk '{print $1}' | sort -u | tr '\n' ', ' | sed 's/,$//')
set -o pipefail
echo "    $DEPS"
echo ""

echo "  File type verification:"
echo "    Static: $(file -b "$OUTDIR/io_static" | head -c80)"
echo "    Dynamic: $(file -b "$OUTDIR/io_dynamic" | head -c80)"
