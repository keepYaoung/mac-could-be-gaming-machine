#!/usr/bin/env bash
# HLSL -> DXIL (dxc) -> metallib (metal-shaderconverter).
#
# Two dxc paths supported (NOTES.md M2):
#   1) native `dxc` on PATH (LunarG Vulkan SDK, or a source build).
#   2) fallback: Windows dxc.exe under an existing Sikarugir/CrossOver wine.
#      This is fine — HLSL->DXIL is a *build-time* step; the resulting DXIL is
#      identical byte-for-byte regardless of host OS. Verified on M4: the
#      wine-dxc path produces the same metallib and the same rendered PPM as
#      the native path (m2_translated.ppm == m3_triangle.ppm, byte-identical).
#
# Env overrides:
#   DXC       : override the dxc invocation (single command or "wine dxc.exe")
#   WINE_BIN  : path to a wine binary (auto-detected from Sikarugir if unset)
#   DXC_EXE   : path to Windows dxc.exe (default: third_party/dxc/bin/x64/dxc.exe)

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/shaders"
OUT="$ROOT/build/shaders"
mkdir -p "$OUT"

command -v metal-shaderconverter >/dev/null || {
    echo "error: metal-shaderconverter not found on PATH (install from Apple Metal Shader Converter pkg)"; exit 1; }

# --- Resolve a dxc invocation ---
if command -v dxc >/dev/null 2>&1; then
    DXC_CMD=(dxc)
else
    # Fallback: bundled Windows dxc.exe run through a wine we can find.
    DXC_EXE_PATH="${DXC_EXE:-$ROOT/third_party/dxc/bin/x64/dxc.exe}"
    if [[ ! -f "$DXC_EXE_PATH" ]]; then
        echo "error: dxc not on PATH and third_party/dxc/bin/x64/dxc.exe missing."
        echo "  Download: https://github.com/microsoft/DirectXShaderCompiler/releases (dxc_*.zip)"
        echo "  Unpack under $ROOT/third_party/dxc/"
        exit 1
    fi
    if [[ -z "${WINE_BIN:-}" ]]; then
        # Sikarugir carries a working wine with a preset prefix + DYLD paths.
        SK_WINE="/Users/${USER}/Applications/Sikarugir/RDR2.app/Contents/SharedSupport/wine/bin/wine"
        if [[ -x "$SK_WINE" ]]; then WINE_BIN="$SK_WINE"; fi
    fi
    if [[ -z "${WINE_BIN:-}" || ! -x "${WINE_BIN}" ]]; then
        echo "error: dxc not on PATH, and no wine found for dxc.exe fallback."
        echo "  Install a Wine (Sikarugir/CrossOver), or set WINE_BIN=/path/to/wine."
        exit 1
    fi
    # Sikarugir's wine needs its own DYLD/prefix env. Derived from the app layout.
    SK_APP="$(dirname "$(dirname "$(dirname "$WINE_BIN")")")"     # .../SharedSupport
    SK_ROOT="$(dirname "$SK_APP")"                                # .../Contents
    export WINEPREFIX="${WINEPREFIX:-$SK_APP/prefix}"
    export DYLD_FALLBACK_LIBRARY_PATH="${DYLD_FALLBACK_LIBRARY_PATH:-$SK_ROOT/Frameworks/moltenvkcx:$SK_APP/wine/lib:$SK_APP/wine/lib64:$SK_ROOT/Frameworks:$SK_ROOT/Frameworks/GStreamer.framework/Libraries:/opt/wine/lib:/usr/lib:/usr/libexec:/usr/lib/system}"
    export WINEDEBUG="${WINEDEBUG:--all}"
    DXC_CMD=("$WINE_BIN" "$DXC_EXE_PATH")
fi

# Allow a full override via DXC="..." if the user wants to.
if [[ -n "${DXC:-}" ]]; then
    # shellcheck disable=SC2206
    DXC_CMD=($DXC)
fi

echo "dxc: ${DXC_CMD[*]}"

# Vertex + pixel stages from the single source file.
"${DXC_CMD[@]}" -T vs_6_0 -E VSMain "$SRC/triangle.hlsl" -Fo "$OUT/triangle_vs.dxil"
"${DXC_CMD[@]}" -T ps_6_0 -E PSMain "$SRC/triangle.hlsl" -Fo "$OUT/triangle_ps.dxil"

# DXIL -> metallib. (DESIGN.md D-1: MSC is the PoC shader path; the open layer
# would instead reuse vkd3d's dxil-spirv front-end. RESEARCH.md §3.3.)
metal-shaderconverter "$OUT/triangle_vs.dxil" -o "$OUT/triangle_vs.metallib"
metal-shaderconverter "$OUT/triangle_ps.dxil" -o "$OUT/triangle_ps.metallib"

echo "ok -> $OUT/{triangle_vs,triangle_ps}.metallib"
