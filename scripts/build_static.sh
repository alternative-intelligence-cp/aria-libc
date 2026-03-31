#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────
# build_static.sh — Build Aria programs statically linked against musl
# ─────────────────────────────────────────────────────────────────────
# Usage:
#   ./scripts/build_static.sh <input.aria> -o <output> [-L<path>] [-l<lib>] ...
#
# Environment variables:
#   ARIAC       — path to ariac compiler     (default: auto-detect)
#   MUSL_DIR    — musl install prefix         (default: build/musl)
#   GCC_LIB     — GCC static lib directory    (default: auto-detect)
#   ARIA_RT     — libaria_runtime.a path      (default: auto-detect)
#   COMPAT_DIR  — compat/ directory           (default: compat/)
#   VERBOSE     — set to 1 for debug output
#
# Example:
#   ./scripts/build_static.sh tests/test_alloc.aria -o test_alloc_static
# ─────────────────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# ── Auto-detect paths ───────────────────────────────────────────────
ARIAC="${ARIAC:-$(realpath "$PROJECT_DIR/../aria/build/ariac" 2>/dev/null || echo "ariac")}"
MUSL_DIR="${MUSL_DIR:-$PROJECT_DIR/build/musl}"
ARIA_RT="${ARIA_RT:-$(realpath "$PROJECT_DIR/../aria/build/libaria_runtime.a" 2>/dev/null || echo "")}"
COMPAT_DIR="${COMPAT_DIR:-$PROJECT_DIR/compat}"
VERBOSE="${VERBOSE:-0}"

# Auto-detect GCC static libs
if [[ -z "${GCC_LIB:-}" ]]; then
    # Find the newest GCC version's lib dir
    GCC_LIB="$(ls -d /usr/lib/gcc/x86_64-linux-gnu/*/ 2>/dev/null | sort -V | tail -1)"
    if [[ -z "$GCC_LIB" ]]; then
        echo "ERROR: Cannot find GCC static libraries. Set GCC_LIB env." >&2
        exit 1
    fi
fi

# ── Validate prerequisites ──────────────────────────────────────────
fail() { echo "ERROR: $*" >&2; exit 1; }

[[ -x "$ARIAC" ]]                || fail "ariac not found at: $ARIAC"
[[ -f "$MUSL_DIR/lib/libc.a" ]]  || fail "musl libc.a not found at: $MUSL_DIR/lib/"
[[ -f "$MUSL_DIR/lib/crt1.o" ]]  || fail "musl crt1.o not found at: $MUSL_DIR/lib/"
[[ -f "$ARIA_RT" ]]               || fail "libaria_runtime.a not found at: $ARIA_RT"
[[ -f "$COMPAT_DIR/libglibc_compat.a" ]] || fail "libglibc_compat.a not found in: $COMPAT_DIR/"
[[ -f "$GCC_LIB/libstdc++.a" ]]  || fail "libstdc++.a not found in: $GCC_LIB"

# ── Parse arguments ─────────────────────────────────────────────────
INPUT_FILES=()
OUTPUT=""
USER_LINK_FLAGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        -o)
            OUTPUT="$2"
            shift 2
            ;;
        -L*|-l*)
            USER_LINK_FLAGS+=("$1")
            shift
            ;;
        -*)
            fail "Unknown flag: $1"
            ;;
        *)
            INPUT_FILES+=("$1")
            shift
            ;;
    esac
done

[[ ${#INPUT_FILES[@]} -gt 0 ]] || fail "No input .aria file(s) specified"
[[ -n "$OUTPUT" ]]              || fail "No output specified (-o <file>)"

# ── Build ────────────────────────────────────────────────────────────
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

ASM_FILE="$TMPDIR/output.s"
OBJ_FILE="$TMPDIR/output.o"

# Step 1: Compile Aria → assembly
[[ "$VERBOSE" == "1" ]] && echo "[1/3] ariac → assembly"
"$ARIAC" "${INPUT_FILES[@]}" --emit-asm -o "$ASM_FILE" 2>/dev/null

# Step 2: Assemble → object
[[ "$VERBOSE" == "1" ]] && echo "[2/3] assembly → object"
clang -c "$ASM_FILE" -o "$OBJ_FILE"

# Step 2b: Rebuild glibc_compat with musl headers for struct layout compatibility
MUSL_COMPAT_DIR="$TMPDIR/musl-compat"
mkdir -p "$MUSL_COMPAT_DIR"
CLANG_BUILTINS="$(clang -print-resource-dir)/include"

if [[ -f "$COMPAT_DIR/glibc_compat.c" ]]; then
    [[ "$VERBOSE" == "1" ]] && echo "  rebuilding glibc_compat with musl headers"
    clang -O2 -Wall -fPIC -c -nostdinc \
        -isystem "$MUSL_DIR/include" \
        -isystem "$CLANG_BUILTINS" \
        "$COMPAT_DIR/glibc_compat.c" -o "$MUSL_COMPAT_DIR/glibc_compat.o"
    ar rcs "$MUSL_COMPAT_DIR/libglibc_compat.a" "$MUSL_COMPAT_DIR/glibc_compat.o"
fi

# Step 3: Static link with musl
[[ "$VERBOSE" == "1" ]] && echo "[3/3] static link (musl + aria runtime)"
clang++ -static -nostdlib -nostartfiles \
    "$MUSL_DIR/lib/crt1.o" \
    "$MUSL_DIR/lib/crti.o" \
    "$OBJ_FILE" \
    "$ARIA_RT" \
    -L"$MUSL_COMPAT_DIR" \
    -L"$COMPAT_DIR" \
    "${USER_LINK_FLAGS[@]}" \
    -lglibc_compat \
    -L"$GCC_LIB" -lstdc++ -lgcc -lgcc_eh -latomic \
    "$MUSL_DIR/lib/libc.a" \
    "$MUSL_DIR/lib/crtn.o" \
    -o "$OUTPUT"

# ── Report ───────────────────────────────────────────────────────────
SIZE=$(stat -c%s "$OUTPUT" 2>/dev/null || stat -f%z "$OUTPUT")
FILE_TYPE=$(file -b "$OUTPUT")

if [[ "$VERBOSE" == "1" ]]; then
    echo "─────────────────────────────────────────"
    echo "Output: $OUTPUT"
    echo "Size:   $SIZE bytes ($(echo "scale=1; $SIZE/1024/1024" | bc) MB)"
    echo "Type:   $FILE_TYPE"
    echo "─────────────────────────────────────────"
fi

# Verify it's actually static
if echo "$FILE_TYPE" | grep -q "statically linked"; then
    [[ "$VERBOSE" == "1" ]] && echo "✓ Verified: statically linked" || true
else
    echo "WARNING: Binary may not be fully static!" >&2
    echo "  file: $FILE_TYPE" >&2
fi
