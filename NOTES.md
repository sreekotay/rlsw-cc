# rlsw-cc notes

Overlay on raylib 6.0 `rlsw` 1.5. Stock header is never edited. Ours lives in `include/rlsw.h` and is copied onto `${raylib_SOURCE_DIR}/src/external/rlsw.h` at build time (`rlgl.h` includes that path; `-I` cannot win). This directory is a standalone CMake subproject — see `README.md`.

## Binaries (rayrender harness)

One software CMake produces both:

| Target | Header | HUD |
| --- | --- | --- |
| `rayrender` | `rlsw-cc/include/rlsw.h` | `impl=cc` |
| `rayrender-stock` | `rlsw-cc/stock/rlsw.h` | `impl=stock` |

```bash
cmake -S . -B build && cmake --build build -j
./tools/rayrender.sh cc      # or stock
./tools/parity.sh --smoke
./tools/parity.sh --bench
./tools/parity.sh --retina --ppm
```

`RAYRENDER_GPU=ON` is a single OpenGL binary, no pair.

## Harness

`RAYRENDER_PARITY=1` hides the window, freezes camera/orbit, hashes `swGetColorBuffer` with FNV-1a 64, prints:

```
rayrender impl=cc seq=1 width=... height=... scene=maze frames=2 checksum=0x... time_ms=...
```

`--smoke` sets `RAYRENDER_ADAPTIVE=0`. After 1.5 (bary planes) maze is allowed to DIFF; `RAYRENDER_STRICT=1` still requires MATCH. Parity also `SetRandomSeed(1)` because maze gen uses `GetRandomValue`.

On a Retina Mac, RGFW software FB is 2× the window, so `--smoke` 320×180 hashes 640×360 and `--bench` 1280×720 hashes 2560×1440.

## Phase log

### 0 — overlay + referee

Vendored rlsw 1.5 into `stock/` and `include/`. Dual targets + HUD `impl=` + `tools/parity.sh`.

Smoke (320×180 maze, q0, adaptive off): checksums **must match**.

### 1.1 — adaptive 1/w

Formula from PB ScanTest / xs: `sqrt(4+3*|w/dw|)`, clamp 4–64. Span `< 16` or `dw≈0` keeps stock block 16. Key `A` in `rayrender` only. Default on.

Untextured / no-stretch: exact vs stock when adaptive is off. Stretch: expect a checksum delta; judge with `--ppm`.

| Run | stock | cc | vs_stock | checksum |
| --- | --- | --- | --- | --- |
| smoke 320×180 (FB 640×360) q0 A=off, 4f | 18.7 ms | 17.7 ms | noise | MATCH `0xe292f7c7dd3aaa06` |
| bench 1280×720 (FB 2560×1440) q2 A=on, 12f | 396 ms | 391 ms | 1.01× | DIFF (stretch; expected) |

Maze walls are mostly near-field; adaptive often stays near 16 so the win is flat. Keep the hook (key `A`) and look at viewer/stress later. Do not treat 1.01× as a ship gate.

### 1.2 — clip-space trivial reject

Homogeneous outcodes before Sutherland–Hodgman. Reject only if every vertex is outside the **same** plane (`w`, `±x`, `±y`, `±z`, scissor). Same zeros as stock clip. Coverage unchanged.

### 1.5 — bary planes + pixel-center snap + 16.16 step

Once per triangle: inverse area, float `dadx`/`dady`, origin = vert with largest `1/w`, snap `round + 0.5 - origin`. Spans step those **float** planes. 16.16 was pulled: `int32 * pixel-delta` overflowed on near-camera `uv/w` (jitter / misaligned walls). Quantize only after range-safe steps (1.6). Edge walk still stock `(int)x`.

| Run | stock | cc | vs_stock | checksum |
| --- | --- | --- | --- | --- |
| smoke 320×180 (FB 640×360) q0 A=off, 2f | 17.5 ms | 13.0 ms | ~1.35× (small) | DIFF `stock=0xe292…` `cc=0xc08a…` |
| bench 1280×720 (FB 2560×1440) q2 A=on, 12f | 397 ms | 387 ms | 1.02× | DIFF |

DIFF is pixel-center snap vs stock span lerp, not a hole hunt. Bench still flat on maze walls.

### 1.6 — 8.24 bary weights

Step screen-space bary `bu,bv` (p1,p2; p3=`1-bu-bv`). Reconstruct `1/w`, z, uv, color as `bu*p1+bv*p2+bw*p3`. 8.24 + int64 step when `|d bary / dxy| < 64`; skinny/steep tris keep float weights. Not 16.16 on `u/w`.

### 1.6b — integer span after `1/w` (opaque + nearest + depth)

Keep float `U,V,W` (already `/w`). Adaptive blocks still do two `1/w`s. Then quantize **texel** UV to 16.16 and color to 8.24 and add those. Nearest is `(u>>16)&(w-1)` on POT+REPEAT RGBA8. Modulate is 8.8×texel (8-bit slop). Clamp / non-POT stay on the float sample path.

| Run | stock | cc | vs_stock |
| --- | --- | --- | --- |
| bench 1280×720 (FB 2560×1440) q2 A=on, 12f | 411 ms | 329 ms | 1.25× |

### 1.6e — packed AG/RB bilerp

Same integer span as nearest (16.16 texel UV after `1/w`, 8.24 color) when min=mag=`LINEAR`. Fetch is packed AG/RB lerp with 8-bit weights and a −0.5 texel center. Early-out if the 2×2 is one color. Clamp / NPOT / mixed min-mag stay on the float sample.

### 1.6d — 8.24 bary along the span

On opaque+tex+depth when bary fits 8.24: step `bu,bv` in int64, reconstruct `U,V,W,C` only at `1/w` knots, then the same 16.16/8.24 inner add. No float `u,v,color` accumulators on that path. Skinny tris keep float weights. Bilinear is the same walk with a different fetch.

### 1.6c — float w-buffer

Depth is screen-linear `1/w` (already the span `w` plane). Closer = larger. Clear far is `0` (`memset`). Drop NDC-z interpolant on triangle spans. Same compare on quads/lines/points so they share the buffer.

### Parked — Sree FPU 2006 / magic ToFix

[Know Your FPU, 2006](https://stereopsis.com/sree/fpu2006.html): `xs_CRoundToInt` / `xs_Fix<N>::ToFix` (add `2^(52-N)*1.5`, read mantissa). Was 2–5× vs C cast on P4 because `(int)f` fought FPU rounding mode.

Microbench on Apple M5 (`cc -O3`):

| Pattern | C cast | magic ToFix | NEON 4-wide |
| --- | --- | --- | --- |
| Packed 2e6 stream | 0.38 ns | ~1.5× faster, ties differ by 1 | ~4.5× |
| Setup-like (12 conv × 200k tris) | 0.087 ns | wash / worse | wash |

**Do not wire into `sw_to_fix16`.** Setup is already `fcvtzs`; optimistic save ~1 ns/tri. Pixels stay float planes until 8.24 bary weights. x86 P4 numbers do not apply.

### 2 — comptime span kernels (triangle pipelines)

Curl-style one seam: GL/clip/edges stay in [`include/rlsw.h`](include/rlsw.h). Integer **span pipelines** live in [`src/fill/span_kernels.ccs`](src/fill/span_kernels.ccs) and ship as checked-in [`include/generated/span_kernels.c`](include/generated/span_kernels.c).

```bash
./rlsw-cc/tools/gen.sh kernels   # ccc --emit-c-only; default cmake never runs this
```

**Not** a Levenshtein-style call table in the pixel/block loop. Each `sw_gen_span_*` is a full span fill with depth/blend/tex/filter/wrap baked in. Triangle templates (`T_d1_b0_f0_w0`, …) edge-walk and call that span **by direct name**. Pick once in `sw_triangle_render` (same shape as PB `Config3d` / rlsw `#include __FILE__` variants).

Matrix (28 pipelines):

| Axis | Values |
| --- | --- |
| depth | off / w-buffer |
| blend | store / packed src-over |
| tex | color-only / RGBA8 |
| filter | nearest / packed AG/RB bilerp |
| wrap | repeat POT / repeat NPOT / clamp |

Leftover float sample: non-RGBA8, mixed S/T wrap, exotic blend. Maze nearest smoke (pre–26.6 edges): `0x5d8c7a6dcddb3438`.

### 2.5 — 26.6 triangle edges (coverage)

Vertex snap `sw_to_26_6` + integer coverage x from endpoints. **Exact** `x = x0 + dx*(y-y0)/dy` per scanline (64-bit); truncated per-scan `dx` drifted inward on tall walls and opened vertical light seams. Sample Y = `(y+1)<<6` (stock `1-fract` prestep). Thin `px1==px0` columns are kept. Attrib planes / span kernels unchanged.

Maze hashes match pre-edge-regression (bary/integer span era): smoke `0x5d8c7a6dcddb3438`, Retina `0x2e13ab57d738130a`.

| Run | stock | cc | vs_stock | checksum |
| --- | --- | --- | --- | --- |
| smoke 320×180 (FB 640×360) q0 A=off, 2f | 17.0 ms | 14.4 ms | ~1.19× | DIFF `cc=0x5d8c7a6dcddb3438` |
| bench 1280×720 (FB 2560×1440) q2 A=on, 12f | 448 ms | 346 ms | **1.29×** | DIFF `cc=0x2e13ab57d738130a` |

Next: sequential tiles (then pigz-wait).

### 3.1 — sequential bins (stripe / tile knob)

Opaque tris go into an owned bin list (copied screen-space verts + raster fn + tex). Flush by bin before blend/lines/points/readback; discard on clear. `swSetBinSize(w,h)`: `h=0` off; `w=0` → full-width **stripe**.

App: default **stripe×64**. `[B]` cycles stripe×64 → 64×64 → 128×128 → off. Env `RAYRENDER_BIN=stripe|off|64x64|128x128`.

| Preset | Retina maze 12f vs stock | vs immediate cc | checksum vs immediate |
| --- | --- | --- | --- |
| **stripe×64 (default)** | **1.30×** | ~same | **MATCH** immediate |
| off | **1.29×** | — | `0x2e13ab57d738130a` |
| 64×64 | ~1.05× | **lose** | DIFF (affine `1/w` block phase at X cuts) |
| 128×128 | ~1.20× | mild loss | DIFF (same reason) |

Stripe is the sequential fork and the pigz-wait base (exact pixels, no slowdown). 2D tiles pay re-setup × overlap and split spans; bit-identical only if we drop affine blocks.

### 3.2 — `@parallel` stripe flush

Planes constructed once into the bin entry; each stripe builds a stack `sw_fill_ctx_t` (clip + pointers, no plane copy). Flush:

```c
@parallel for (ty in 0..nty) { sw_bin_fill_cell(0, ty, &a); }  // bin_par.ccs
```

Checked-in emit: [`include/generated/bin_par.c`](include/generated/bin_par.c). CMake links Concurrent-C runtime (`ccc --print-libs`); does **not** run `ccc`.

```bash
./rlsw-cc/tools/gen.sh bin_par   # only when changing bin_par.ccs
```

`RAYRENDER_SEQ=1` / `[S]` → same sequential nested loop. SEQ and PAR stripe checksums must MATCH.
