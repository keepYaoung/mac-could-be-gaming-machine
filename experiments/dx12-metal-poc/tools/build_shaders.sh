#!/usr/bin/env bash
# HLSL -> DXIL (dxc) -> metallib (metal-shaderconverter)
# Requires (NOTES.md M0): dxc on PATH, metal-shaderconverter on PATH.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/shaders"
OUT="$ROOT/build/shaders"
mkdir -p "$OUT"

command -v dxc >/dev/null               || { echo "error: dxc not found on PATH (M0)"; exit 1; }
command -v metal-shaderconverter >/dev/null || { echo "error: metal-shaderconverter not found on PATH (M0)"; exit 1; }

# Vertex + pixel stages from the single source file.
dxc -T vs_6_0 -E VSMain "$SRC/triangle.hlsl" -Fo "$OUT/triangle_vs.dxil"
dxc -T ps_6_0 -E PSMain "$SRC/triangle.hlsl" -Fo "$OUT/triangle_ps.dxil"

# DXIL -> metallib. (DESIGN.md D-1: MSC is the PoC shader path; the open layer
# would instead reuse vkd3d's dxil-spirv front-end. RESEARCH.md §3.3.)
metal-shaderconverter "$OUT/triangle_vs.dxil" -o "$OUT/triangle_vs.metallib"
metal-shaderconverter "$OUT/triangle_ps.dxil" -o "$OUT/triangle_ps.metallib"

echo "ok -> $OUT/{triangle_vs,triangle_ps}.metallib"
