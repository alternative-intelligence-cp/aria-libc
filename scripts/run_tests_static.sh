#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────
# run_tests_static.sh — Build & run the full aria-libc test suite
#                        using static musl-linked binaries
# ─────────────────────────────────────────────────────────────────────
# Usage:
#   ./scripts/run_tests_static.sh          # run all tests
#   ./scripts/run_tests_static.sh io math  # run only io and math
# ─────────────────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_STATIC="$SCRIPT_DIR/build_static.sh"

cd "$PROJECT_DIR"

# ── Test definitions ─────────────────────────────────────────────────
# Format: test_name:aria_file:link_flags
declare -a ALL_TESTS=(
    "io:tests/test_libc_io.aria:-laria_libc_io"
    "mem:tests/test_libc_mem.aria:-laria_libc_mem"
    "string:tests/test_libc_string.aria:-laria_libc_string -laria_libc_mem"
    "math:tests/test_libc_math.aria:-laria_libc_math"
    "time:tests/test_libc_time.aria:-laria_libc_time"
    "process:tests/test_libc_process.aria:-laria_libc_process"
    "use_import:tests/test_use_import.aria:-laria_libc_io"
    "use_mem:tests/test_use_mem.aria:-laria_libc_mem"
    "use_string:tests/test_use_string.aria:-laria_libc_string -laria_libc_mem"
    "use_math:tests/test_use_math.aria:-laria_libc_math"
    "use_time:tests/test_use_time.aria:-laria_libc_time"
    "use_process:tests/test_use_process.aria:-laria_libc_process"
    "net:tests/test_libc_net.aria:-laria_libc_net"
    "use_net:tests/test_use_net.aria:-laria_libc_net"
    "posix:tests/test_libc_posix.aria:-laria_libc_posix"
    "use_posix:tests/test_use_posix.aria:-laria_libc_posix"
    "fs:tests/test_libc_fs.aria:-laria_libc_fs -laria_libc_io"
    "use_fs:tests/test_use_fs.aria:-laria_libc_fs -laria_libc_io"
    "regex:tests/test_libc_regex.aria:-laria_libc_regex"
    "use_regex:tests/test_use_regex.aria:-laria_libc_regex"
    # v0.2.1: Pure Aria sys() modules (no C shim link flags except where noted)
    "errno:tests/test_errno.aria:"
    "identity:tests/test_identity.aria:"
    "io_core:tests/test_io_core.aria:"
    "stat:tests/test_stat.aria:-laria_libc_mem"
    # v0.2.2: Pure Aria sys() filesystem modules
    "fs_core:tests/test_fs_core.aria:"
    "fs_link:tests/test_fs_link.aria:-laria_libc_mem"
    "fs_dir:tests/test_fs_dir.aria:"
    "fs_readdir:tests/test_fs_readdir.aria:-laria_libc_mem"
    # v0.2.3: Pure Aria sys() memory management
    "mmap:tests/test_mmap.aria:-laria_libc_mem"
    "alloc:tests/test_alloc.aria:-laria_libc_mem"
    "buf:tests/test_buf.aria:-laria_libc_mem"
    # v0.2.4: Pure Aria sys() time + clock
    "time_pure:tests/test_time_pure.aria:-laria_libc_mem"
    "time_fmt:tests/test_time_fmt.aria:"
)

# ── Filter tests if args provided ───────────────────────────────────
if [[ $# -gt 0 ]]; then
    SELECTED=("$@")
    TESTS=()
    for entry in "${ALL_TESTS[@]}"; do
        name="${entry%%:*}"
        for sel in "${SELECTED[@]}"; do
            if [[ "$name" == *"$sel"* ]]; then
                TESTS+=("$entry")
                break
            fi
        done
    done
else
    TESTS=("${ALL_TESTS[@]}")
fi

# ── Build output dir ────────────────────────────────────────────────
OUTDIR="$PROJECT_DIR/build/static-tests"
mkdir -p "$OUTDIR"

# ── Run tests ────────────────────────────────────────────────────────
TOTAL=0
PASSED=0
FAILED=0
FAIL_LIST=()

echo "═══════════════════════════════════════════════════════════════"
echo "  aria-libc Static Test Suite (musl-linked)"
echo "═══════════════════════════════════════════════════════════════"
echo ""

for entry in "${TESTS[@]}"; do
    IFS=: read -r name aria_file link_flag <<< "$entry"
    TOTAL=$((TOTAL + 1))
    
    BIN="$OUTDIR/$name"
    
    printf "  %-20s " "[$name]"
    
    # Build
    if ! "$BUILD_STATIC" "$aria_file" -o "$BIN" -Lshim $link_flag 2>/tmp/static_build_err; then
        echo "BUILD FAIL"
        FAILED=$((FAILED + 1))
        FAIL_LIST+=("$name (build)")
        [[ -s /tmp/static_build_err ]] && cat /tmp/static_build_err | head -5
        continue
    fi
    
    # Verify static
    if ! file "$BIN" | grep -q "statically linked"; then
        echo "NOT STATIC"
        FAILED=$((FAILED + 1))
        FAIL_LIST+=("$name (not static)")
        continue
    fi
    
    # Run (from tests/ directory so relative paths in tests resolve correctly)
    if OUTPUT=$(cd "$PROJECT_DIR/tests" && "$BIN" 2>&1); then
        # Count individual PASS/FAIL from output (use -o since output may be single-line)
        # Temporarily disable pipefail for grep pipelines that may have 0 matches
        set +o pipefail
        PASSES=$(echo "$OUTPUT" | grep -o "PASS:" | wc -l)
        FAILS=$(echo "$OUTPUT" | grep -o "FAIL:" | wc -l)
        set -o pipefail
        # Subtract the summary lines ("PASS: N" and "FAIL: 0") from counts
        if echo "$OUTPUT" | grep -q "FAIL: 0"; then
            FAILS=$((FAILS > 0 ? FAILS - 1 : 0))
        fi
        PASSES=$((PASSES > 0 ? PASSES - 1 : 0))
        SIZE=$(stat -c%s "$BIN" 2>/dev/null || stat -f%z "$BIN")
        SIZE_KB=$((SIZE / 1024))
        
        if [[ "$FAILS" -gt 0 ]]; then
            echo "FAIL ($FAILS failures, $PASSES passes) [${SIZE_KB}KB]"
            FAILED=$((FAILED + 1))
            FAIL_LIST+=("$name (test)")
        else
            echo "PASS ($PASSES tests) [${SIZE_KB}KB static]"
            PASSED=$((PASSED + 1))
        fi
    else
        echo "RUNTIME FAIL (exit $?)"
        FAILED=$((FAILED + 1))
        FAIL_LIST+=("$name (runtime)")
    fi
done

echo ""
echo "═══════════════════════════════════════════════════════════════"
echo "  Results: $PASSED/$TOTAL passed, $FAILED failed"
echo "═══════════════════════════════════════════════════════════════"

if [[ $FAILED -gt 0 ]]; then
    echo ""
    echo "  Failed tests:"
    for f in "${FAIL_LIST[@]}"; do
        echo "    - $f"
    done
    exit 1
fi

echo ""
echo "  All tests pass as fully static musl-linked binaries!"
exit 0
