# rlsw-cc

Optimized fork of raylib’s **rlsw** software rasterizer (OpenGL 1.1-style) in [Concurrent-C](https://github.com/sreekotay/concurrent-c) — a strict C11-superset preprocessor: `.ccs` lowers to plain C and compiles with your host C compiler. Span kernels and `@parallel` stripe fill ship as checked-in C plus a portable runtime snapshot.

**Showcase / harness:** [rayrender](https://github.com/sreekotay/rayrender) — FetchContents raylib 6.0 + this package, overlays `rlsw.h`, dual binaries vs stock rlsw 1.5, parity benches.

Drop this directory into a project (or FetchContent / submodule it) and overlay onto raylib 6.0 **Software / RGFW**.

**Consumers do not need `ccc`.** Parallel support uses the checked-in `vendor/cccportable` host-C snapshot.

## Performance vs stock rlsw 1.5

Apple Silicon (M-series), maze scene, 12 frames, adaptive on, **hstripe×64** parallel fill, quality 2. Measured with [rayrender](https://github.com/sreekotay/rayrender) `./tools/parity.sh`:

| Mode | Window (draw FB) | Filter | vs stock |
| --- | --- | --- | ---: |
| bench | 1280×720 (2560×1440) | point | **~1.6–1.8×** |
| bench | 1280×720 (2560×1440) | bilinear | **~2.0–2.2×** |
| retina | 2560×1440 (5120×2880) | point | **~1.7–1.9×** |
| retina | 2560×1440 (5120×2880) | bilinear | **~2.3–2.4×** |

Checksums intentionally DIFF vs stock after bary-plane / integer-span work; cc hashes are stable across SEQ/PAR hstripe (e.g. bench point `0x2e13ab57d738130a`).

## Layout

| Path | Role |
| --- | --- |
| `include/rlsw.h` | Fork (replaces raylib `src/external/rlsw.h`) |
| `include/generated/*.c` | Checked-in emits (`span_kernels` `#include`d from `rlsw.h`; `bin_par` own TU) |
| `vendor/cccportable/` | Host-C headers + runtime (`ccc portable-install`) |
| `stock/rlsw.h` | Untouched rlsw 1.5 referee |
| `src/fill/*.ccs` | Concurrent-C sources (authors only) |
| `CMakeLists.txt` | Overlay helper + `rlsw_cc` library |

## CMake (parent already has raylib)

```cmake
# After FetchContent_MakeAvailable(raylib) with OPENGL_VERSION=Software, PLATFORM=RGFW:
add_subdirectory(rlsw-cc)   # or FetchContent this repo
rlsw_cc_overlay_raylib(raylib "${raylib_SOURCE_DIR}")

add_executable(my_app ...)
target_link_libraries(my_app PRIVATE raylib rlsw_cc)
```

**Why overlay, not `-I`?** `rlgl.h` does `#include "external/rlsw.h"` relative to raylib’s `src/`. Include paths cannot override that.

`rlsw_cc` = `bin_par.c` + `vendor/cccportable/runtime/concurrent_c.c`. Link it on the **app**, not on `raylib`.

### Options

| Option | Default | Meaning |
| --- | --- | --- |
| `RLSW_CC_PARALLEL` | `ON` | `@parallel` stripes via checked-in `vendor/cccportable` (no `ccc`) |
| `RLSW_CC_USE_STOCK` | `OFF` | Overlay `stock/rlsw.h` instead of the fork |

## Authors (have `ccc`)

```bash
./tools/gen.sh            # emit --no-line into include/generated/
./tools/vendor-ccc.sh     # refresh vendor/cccportable
```

CMake never runs `ccc`. Kernels use `#pragma(@prelude) off` + `#ifdef CC_PARSER_MODE` host stubs (inert inside `rlsw.h`).

## Extra APIs (vs stock rlsw)

- `swSetAdaptiveAffine(bool)` — vary `1/w` block size
- `swSetBinSize(int w, int h)` — bins / stripes (`w=0` → hstripe; `h=0` → vstripe)
- `swSetSeq(bool)` — force sequential stripe fill

See `NOTES.md` for the phase log. End-to-end benches and HUD live in [rayrender](https://github.com/sreekotay/rayrender).
