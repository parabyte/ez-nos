#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build/watcom}"
OBJ_DIR="$BUILD_DIR/obj"
BIN_DIR="$BUILD_DIR/bin"
GEN_DIR="$BUILD_DIR/generated"
MANIFEST_FILE="$BUILD_DIR/manifest.txt"
EXE_PATH="$BIN_DIR/eznos.exe"

usage() {
    cat <<'EOF'
Usage: scripts/build_watcom.sh [--clean]

Environment variables:
  BUILD_DIR          Override the build staging directory (default: build/watcom)
  WATCOM_CC          Override the Watcom C compiler executable (default: wcc)
  WATCOM_ASM         Override the Watcom assembler executable (default: wasm)
  WATCOM_LINKER      Override the Watcom linker/driver (default: wcl)
  WATCOM_CFLAGS      Extra flags passed to the C compiler
  WATCOM_ASMFLAGS    Extra flags passed to the assembler
EOF
}

CLEAN=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean)
            CLEAN=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "error: unknown option '$1'" >&2
            usage
            exit 2
            ;;
    esac
done

if (( CLEAN )); then
    rm -rf "$BUILD_DIR"
    echo "Watcom build artifacts removed from $BUILD_DIR"
    exit 0
fi

for tool in python3 "${WATCOM_CC:-wcc}" "${WATCOM_ASM:-wasm}" "${WATCOM_LINKER:-wcl}"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "error: required tool '$tool' not found in PATH" >&2
        exit 1
    fi
done

if [[ -z "${WATCOM:-}" ]]; then
    echo "warning: WATCOM environment variable is not set. Did you source owsetenv.sh?" >&2
fi

if [[ ! -f "$PROJECT_ROOT/global.h" ]]; then
    cat >&2 <<'EOF'
error: global.h is missing from the project root.
The original EZNOS archives ship this header; copy it into place before building.
EOF
    exit 1
fi

if [[ ! -f "$PROJECT_ROOT/pc.h" ]]; then
    echo "error: pc.h is required but was not found at the project root" >&2
    exit 1
fi

mkdir -p "$OBJ_DIR" "$BIN_DIR" "$GEN_DIR"

echo "[watcom] Resolving source list from ALTERED/MAKEFILE/MAKEFILE"
if ! "$PROJECT_ROOT/scripts/watcom_manifest.py" --root "$PROJECT_ROOT" --output "$MANIFEST_FILE"; then
    echo "Watcom manifest generation failed; see errors above." >&2
    exit 1
fi

if [[ ! -s "$MANIFEST_FILE" ]]; then
    echo "error: empty manifest generated; nothing to build" >&2
    exit 1
fi

cp "$PROJECT_ROOT/pc.h" "$GEN_DIR/hardware.h"

WCC_BIN="${WATCOM_CC:-wcc}"
WASM_BIN="${WATCOM_ASM:-wasm}"
WCL_BIN="${WATCOM_LINKER:-wcl}"
DEFAULT_CFLAGS="-zq -bt=dos -ml -dMSDOS -d__MSDOS__ -dLARGEDATA"
WCC_FLAGS="${WATCOM_CFLAGS:-$DEFAULT_CFLAGS}"
INCLUDE_FLAGS=(
    "-i=$GEN_DIR"
    "-i=$PROJECT_ROOT"
    "-i=$PROJECT_ROOT/ALTERED"
    "-i=$PROJECT_ROOT/ALTERED/CONFIG"
)
DEFAULT_ASMFLAGS="-zq -bt=dos -dMEMMOD=LARGE"
WASM_FLAGS="${WATCOM_ASMFLAGS:-$DEFAULT_ASMFLAGS}"

OBJ_FILES=()

while IFS='|' read -r obj relsrc; do
    [[ -z "$obj" ]] && continue
    src="$relsrc"
    out="$OBJ_DIR/$obj"
    OBJ_FILES+=("$out")
    if [[ "$out" -nt "$src" ]]; then
        continue
    fi
    echo "[cc ] $src"
    "$WCC_BIN" $WCC_FLAGS "${INCLUDE_FLAGS[@]}" -fo="$out" "$src"
done <"$MANIFEST_FILE"

mapfile -t ASM_SOURCES < <(find "$PROJECT_ROOT" -maxdepth 1 -type f \( -iname '*.asm' \) | sort)
for asm in "${ASM_SOURCES[@]}"; do
    base="$(basename "${asm%.*}")"
    out="$OBJ_DIR/${base}.obj"
    OBJ_FILES+=("$out")
    if [[ "$out" -nt "$asm" ]]; then
        continue
    fi
    echo "[asm] $asm"
    "$WASM_BIN" $WASM_FLAGS -fo="$out" "$asm"
done

if [[ ${#OBJ_FILES[@]} -eq 0 ]]; then
    echo "error: no object files were produced" >&2
    exit 1
fi

EMM_LIB=""
if [[ -f "$PROJECT_ROOT/EMMLIBL.BIL" ]]; then
    cp "$PROJECT_ROOT/EMMLIBL.BIL" "$BUILD_DIR/emmlibl.lib"
    EMM_LIB="$BUILD_DIR/emmlibl.lib"
fi

OBJ_RESP="$BUILD_DIR/objects.rsp"
printf '"%s"\n' "${OBJ_FILES[@]}" >"$OBJ_RESP"

echo "[ld ] $EXE_PATH"
if [[ -n "$EMM_LIB" ]]; then
    "$WCL_BIN" -q -bcl=dos -ml -fe="$EXE_PATH" @"$OBJ_RESP" "$EMM_LIB"
else
    "$WCL_BIN" -q -bcl=dos -ml -fe="$EXE_PATH" @"$OBJ_RESP"
fi

echo "Watcom build complete -> $EXE_PATH"
