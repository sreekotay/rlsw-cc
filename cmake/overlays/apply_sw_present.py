#!/usr/bin/env python3
"""Apply rlsw-cc software-present overlay onto a raylib + RGFW source tree.

Idempotent. Goals:
  - macOS: swGetColorBuffer → staged CGImage present (macos_sw_present.c)
  - Linux/Windows: BGRA surface matching native; RGFW blit presents surface->data
    directly (no channel convert when formats match)
"""

from __future__ import annotations

import pathlib
import re
import sys

MARKER = "RAYRENDER_FAST_PRESENT"

OLD_SWAP_STOCK = r"""            // copy rlsw pixel data to the surface framebuffer
            swReadPixels(0, 0, platform.surfaceWidth, platform.surfaceHeight, SW_RGBA, SW_UNSIGNED_BYTE, platform.surfacePixels);

            // Mac wants a different pixel order. I cant seem to get this to work any other way
            #if defined(__APPLE__)
                unsigned char temp = 0;
                unsigned char *p = NULL;
                for (int i = 0; i < (platform.surfaceWidth * platform.surfaceHeight); i += 1)
                {
                    p = platform.surfacePixels + (i * 4);
                    temp = p[0];
                    p[0] = p[2];
                    p[2] = temp;
                }
            #endif

            // blit surface to the window
            RGFW_window_blitSurface(platform.window, platform.surface);"""

OLD_SWAP_MACOS = r"""            // copy rlsw pixel data to the surface framebuffer (RGBA; no channel scramble)
            swReadPixels(0, 0, platform.surfaceWidth, platform.surfaceHeight, SW_RGBA, SW_UNSIGNED_BYTE, platform.surfacePixels);

            #if defined(__APPLE__)
                /* RAYRENDER_MACOS_PRESENT: CGImage present (see cmake/overlays/macos_sw_present.c) */
                extern void RayrenderMacOSPresent(void *nsview, void *pixels, int width, int height);
                RayrenderMacOSPresent(RGFW_window_getView_OSX(platform.window),
                                      platform.surfacePixels,
                                      platform.surfaceWidth,
                                      platform.surfaceHeight);
            #else
                RGFW_window_blitSurface(platform.window, platform.surface);
            #endif"""

NEW_SWAP = r"""            /* RLSW_CC_SOFT_PRESENT: macOS staged CGImage; else BGRA readback + blit */
            #if defined(__APPLE__)
                /* Present GL bottom-up RGBA via staged CGImage (see macos_sw_present.c). */
                {
                    int cw = 0, ch = 0;
                    void *fb = swGetColorBuffer(&cw, &ch);
                    if (fb != NULL)
                    {
                        extern void RayrenderMacOSPresent(void *nsview, void *pixels, int width, int height);
                        RayrenderMacOSPresent(RGFW_window_getView_OSX(platform.window), fb, cw, ch);
                    }
                }
            #else
                swReadPixels(0, 0, platform.surfaceWidth, platform.surfaceHeight, SW_RGBA, SW_UNSIGNED_BYTE, platform.surfacePixels);
                /* BGRA surface + SW_FRAMEBUFFER_OUTPUT_BGRA → RGFW zero-copy blit */
                RGFW_window_blitSurface(platform.window, platform.surface);
            #endif"""

# Prior forms (migrate forward)
OLD_SWAP_ZEROCOPY = r"""            /* RAYRENDER_FAST_PRESENT: macOS zero-copy FB; else BGRA readback + blit */
            #if defined(__APPLE__)
                /* Present GL bottom-up RGBA via CGImage + layer Y-flip (no ReadPixels copy). */
                {
                    int cw = 0, ch = 0;
                    void *fb = swGetColorBuffer(&cw, &ch);
                    if (fb != NULL)
                    {
                        extern void RayrenderMacOSPresent(void *nsview, void *pixels, int width, int height);
                        RayrenderMacOSPresent(RGFW_window_getView_OSX(platform.window), fb, cw, ch);
                    }
                }
            #else
                swReadPixels(0, 0, platform.surfaceWidth, platform.surfaceHeight, SW_RGBA, SW_UNSIGNED_BYTE, platform.surfacePixels);
                /* BGRA surface + SW_FRAMEBUFFER_OUTPUT_BGRA → RGFW zero-copy blit */
                RGFW_window_blitSurface(platform.window, platform.surface);
            #endif"""

# Prior fast-present form (ReadPixels then Mac present)
OLD_SWAP_FAST = r"""            /* RAYRENDER_FAST_PRESENT: BGRA readback + platform fast blit */
            swReadPixels(0, 0, platform.surfaceWidth, platform.surfaceHeight, SW_RGBA, SW_UNSIGNED_BYTE, platform.surfacePixels);

            #if defined(__APPLE__)
                extern void RayrenderMacOSPresent(void *nsview, void *pixels, int width, int height);
                RayrenderMacOSPresent(RGFW_window_getView_OSX(platform.window),
                                      platform.surfacePixels,
                                      platform.surfaceWidth,
                                      platform.surfaceHeight);
            #else
                /* BGRA surface + SW_FRAMEBUFFER_OUTPUT_BGRA → RGFW zero-copy blit */
                RGFW_window_blitSurface(platform.window, platform.surface);
            #endif"""

# Also match wording variant
OLD_SWAP_FAST2 = r"""            /* RAYRENDER_FAST_PRESENT: platform-aligned readback + fast blit */
            swReadPixels(0, 0, platform.surfaceWidth, platform.surfaceHeight, SW_RGBA, SW_UNSIGNED_BYTE, platform.surfacePixels);

            #if defined(__APPLE__)
                extern void RayrenderMacOSPresent(void *nsview, void *pixels, int width, int height);
                RayrenderMacOSPresent(RGFW_window_getView_OSX(platform.window),
                                      platform.surfacePixels,
                                      platform.surfaceWidth,
                                      platform.surfaceHeight);
            #else
                /* BGRA surface + SW_FRAMEBUFFER_OUTPUT_BGRA → RGFW memcpy/zero-copy blit */
                RGFW_window_blitSurface(platform.window, platform.surface);
            #endif"""

# Match a single createSurface assignment (any indent, RGBA or BGRA).
CREATE_RE = re.compile(
    r"^[ \t]*platform\.surface = RGFW_window_createSurface\("
    r"platform\.window, platform\.surfacePixels, "
    r"platform\.surfaceWidth, platform\.surfaceHeight, "
    r"RGFW_format(?:RGBA|BGRA)8\);[ \t]*$",
    re.M,
)

# Match a previously applied ifdef createSurface block (legacy Apple/else).
CREATE_IF_RE = re.compile(
    r"[ \t]*#if defined\(__APPLE__\) /\* RAYRENDER_FAST_PRESENT \*/\n"
    r"(?:[ \t]*#if defined\(__APPLE__\) /\* RAYRENDER_FAST_PRESENT \*/\n)?"
    r"[ \t]*platform\.surface = RGFW_window_createSurface\([^;]+;\n"
    r"[ \t]*#else\n"
    r"[ \t]*platform\.surface = RGFW_window_createSurface\([^;]+;\n"
    r"[ \t]*#endif\n"
    r"(?:[ \t]*#else\n"
    r"[ \t]*platform\.surface = RGFW_window_createSurface\([^;]+;\n"
    r"[ \t]*#endif\n)?",
    re.M,
)

# Match comment + BGRA createSurface (current form).
CREATE_FAST_RE = re.compile(
    r"[ \t]*/\* RAYRENDER_FAST_PRESENT \*/\n"
    r"[ \t]*platform\.surface = RGFW_window_createSurface\("
    r"platform\.window, platform\.surfacePixels, "
    r"platform\.surfaceWidth, platform\.surfaceHeight, "
    r"RGFW_formatBGRA8\);[ \t]*\n?",
    re.M,
)

X11_BLIT_OLD = r"""void RGFW_FUNC(RGFW_window_blitSurface) (RGFW_window* win, RGFW_surface* surface) {
	RGFW_ASSERT(surface != NULL);
	surface->native.bitmap->data = (char*)surface->native.buffer;
	RGFW_copyImageData((u8*)surface->native.buffer, surface->w, RGFW_MIN(win->h, surface->h), surface->native.format, surface->data, surface->format, surface->convertFunc);

	XPutImage(_RGFW->display, win->src.window, win->src.gc, surface->native.bitmap, 0, 0, 0, 0, (u32)RGFW_MIN(win->w, surface->w), (u32)RGFW_MIN(win->h, surface->h));
	surface->native.bitmap->data = NULL;
	return;
}"""

X11_BLIT_NEW = r"""void RGFW_FUNC(RGFW_window_blitSurface) (RGFW_window* win, RGFW_surface* surface) {
	/* RAYRENDER_FAST_PRESENT: zero-copy XPutImage when formats already match */
	RGFW_ASSERT(surface != NULL);
	if (surface->format == surface->native.format) {
		surface->native.bitmap->data = (char*)surface->data;
	} else {
		surface->native.bitmap->data = (char*)surface->native.buffer;
		RGFW_copyImageData((u8*)surface->native.buffer, surface->w, RGFW_MIN(win->h, surface->h), surface->native.format, surface->data, surface->format, surface->convertFunc);
	}

	XPutImage(_RGFW->display, win->src.window, win->src.gc, surface->native.bitmap, 0, 0, 0, 0, (u32)RGFW_MIN(win->w, surface->w), (u32)RGFW_MIN(win->h, surface->h));
	surface->native.bitmap->data = NULL;
	return;
}"""

WL_BLIT_OLD = r"""void RGFW_FUNC(RGFW_window_blitSurface) (RGFW_window* win, RGFW_surface* surface) {
	RGFW_ASSERT(surface != NULL);

	surface->native.wl_buffer = wl_shm_pool_create_buffer(surface->native.pool, 0, RGFW_MIN(win->w, surface->w), RGFW_MIN(win->h, surface->h), (i32)surface->w * 4, WL_SHM_FORMAT_ARGB8888);
	RGFW_copyImageData(surface->native.buffer, surface->w, RGFW_MIN(win->h, surface->h), surface->native.format, surface->data, surface->format, surface->convertFunc);

	wl_surface_attach(win->src.surface, surface->native.wl_buffer, 0, 0);
	wl_surface_damage(win->src.surface, 0, 0, RGFW_MIN(win->w, surface->w), RGFW_MIN(win->h, surface->h));
	wl_surface_commit(win->src.surface);

	wl_buffer_destroy(surface->native.wl_buffer);
}"""

WL_BLIT_NEW = r"""void RGFW_FUNC(RGFW_window_blitSurface) (RGFW_window* win, RGFW_surface* surface) {
	/* RAYRENDER_FAST_PRESENT: skip channel convert; memcpy only if data != shm */
	RGFW_ASSERT(surface != NULL);

	surface->native.wl_buffer = wl_shm_pool_create_buffer(surface->native.pool, 0, RGFW_MIN(win->w, surface->w), RGFW_MIN(win->h, surface->h), (i32)surface->w * 4, WL_SHM_FORMAT_ARGB8888);
	{
		i32 rows = RGFW_MIN(win->h, surface->h);
		if (surface->format == surface->native.format) {
			if (surface->data != surface->native.buffer)
				RGFW_MEMCPY(surface->native.buffer, surface->data, (size_t)surface->w * (size_t)rows * 4u);
		} else {
			RGFW_copyImageData(surface->native.buffer, surface->w, rows, surface->native.format, surface->data, surface->format, surface->convertFunc);
		}
	}

	wl_surface_attach(win->src.surface, surface->native.wl_buffer, 0, 0);
	wl_surface_damage(win->src.surface, 0, 0, RGFW_MIN(win->w, surface->w), RGFW_MIN(win->h, surface->h));
	wl_surface_commit(win->src.surface);

	wl_buffer_destroy(surface->native.wl_buffer);
}"""

WIN_BLIT_OLD = r"""void RGFW_window_blitSurface(RGFW_window* win, RGFW_surface* surface) {
	RGFW_copyImageData(surface->native.bitmapBits, surface->w, RGFW_MIN(win->h, surface->h), surface->native.format, surface->data, surface->format, surface->convertFunc);
	BitBlt(win->src.hdc, 0, 0, RGFW_MIN(win->w, surface->w), RGFW_MIN(win->h, surface->h), surface->native.hdcMem, 0, 0, SRCCOPY);
}"""

WIN_BLIT_NEW = r"""void RGFW_window_blitSurface(RGFW_window* win, RGFW_surface* surface) {
	/* RAYRENDER_FAST_PRESENT: SetDIBitsToDevice from surface->data when formats match */
	i32 bw = RGFW_MIN(win->w, surface->w);
	i32 bh = RGFW_MIN(win->h, surface->h);
	if (surface->format == surface->native.format) {
		BITMAPINFO bmi;
		RGFW_MEMSET(&bmi, 0, sizeof(bmi));
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = surface->w;
		bmi.bmiHeader.biHeight = -bh; /* top-down; swReadPixels already Y-flips */
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;
		SetDIBitsToDevice(win->src.hdc, 0, 0, bw, bh, 0, 0, 0, bh, surface->data, &bmi, DIB_RGB_COLORS);
	} else {
		RGFW_copyImageData(surface->native.bitmapBits, surface->w, bh, surface->native.format, surface->data, surface->format, surface->convertFunc);
		BitBlt(win->src.hdc, 0, 0, bw, bh, surface->native.hdcMem, 0, 0, SRCCOPY);
	}
}"""


def surface_block(indent: str) -> str:
    # BGRA everywhere: matches SW_FRAMEBUFFER_OUTPUT_BGRA and CA/X11/Win native.
    return (
        f"{indent}/* RAYRENDER_FAST_PRESENT */\n"
        f"{indent}platform.surface = RGFW_window_createSurface(platform.window, platform.surfacePixels, "
        f"platform.surfaceWidth, platform.surfaceHeight, RGFW_formatBGRA8);\n"
    )


def patch_surfaces(text: str) -> tuple[str, int]:
    """Replace createSurface assignments (or repair mangled ifdefs) with BGRA fast-present lines."""

    def collapse_if(m: re.Match[str]) -> str:
        indent_m = re.search(r"^( +)platform\.surface =", m.group(0), re.M)
        indent = indent_m.group(1) if indent_m else "        "
        return f"{indent}platform.surface = RGFW_window_createSurface(platform.window, platform.surfacePixels, platform.surfaceWidth, platform.surfaceHeight, RGFW_formatBGRA8);\n"

    def collapse_fast(m: re.Match[str]) -> str:
        indent_m = re.search(r"^( +)platform\.surface =", m.group(0), re.M)
        indent = indent_m.group(1) if indent_m else "        "
        return f"{indent}platform.surface = RGFW_window_createSurface(platform.window, platform.surfacePixels, platform.surfaceWidth, platform.surfaceHeight, RGFW_formatBGRA8);\n"

    text2 = CREATE_IF_RE.sub(collapse_if, text)
    text2 = CREATE_FAST_RE.sub(collapse_fast, text2)
    count = 0

    def repl(m: re.Match[str]) -> str:
        nonlocal count
        count += 1
        indent = re.match(r"[ \t]*", m.group(0)).group(0)  # type: ignore[union-attr]
        return surface_block(indent).rstrip("\n")

    text3 = CREATE_RE.sub(repl, text2)
    return text3, count


def patch_rcore(path: pathlib.Path) -> list[str]:
    text = path.read_text(encoding="utf-8")
    notes: list[str] = []

    if "swGetColorBuffer" in text and "RLSW_CC_SOFT_PRESENT: macOS staged" in text:
        notes.append("swap ok")
    elif "swGetColorBuffer" in text and OLD_SWAP_ZEROCOPY in text:
        text = text.replace(OLD_SWAP_ZEROCOPY, NEW_SWAP, 1)
        notes.append("swap from zero-copy")
    else:
        for label, old in (
            ("stock-swap", OLD_SWAP_STOCK),
            ("macos-swap", OLD_SWAP_MACOS),
            ("fast-swap", OLD_SWAP_FAST),
            ("fast-swap2", OLD_SWAP_FAST2),
            ("zero-copy", OLD_SWAP_ZEROCOPY),
        ):
            if old in text:
                text = text.replace(old, NEW_SWAP, 1)
                notes.append(f"swap from {label}")
                break
        else:
            if "RayrenderMacOSPresent" not in text or "swGetColorBuffer" not in text:
                raise SystemExit(f"error: SwapScreenBuffer block not found in {path}")
            notes.append("swap ok")

    text, n_surf = patch_surfaces(text)
    notes.append(f"surface x{n_surf}")
    create_markers = text.count("/* RAYRENDER_FAST_PRESENT */\n") + text.count("/* RLSW_CC_SOFT_PRESENT */\n")
    if n_surf < 2 and create_markers < 2:
        raise SystemExit(f"error: surface create sites: got {n_surf} rewrites, {create_markers} markers in {path}")
    if create_markers >= 2:
        notes.append(f"surface markers ok ({create_markers})")

    path.write_text(text, encoding="utf-8")
    return notes


def patch_rgfw(path: pathlib.Path) -> list[str]:
    text = path.read_text(encoding="utf-8")
    notes: list[str] = []
    for label, old, new, key in (
        ("x11-blit", X11_BLIT_OLD, X11_BLIT_NEW, "zero-copy XPutImage"),
        ("wayland-blit", WL_BLIT_OLD, WL_BLIT_NEW, "skip channel convert"),
        ("win-blit", WIN_BLIT_OLD, WIN_BLIT_NEW, "SetDIBitsToDevice from surface"),
    ):
        if key in text:
            notes.append(f"{label} ok")
        elif old in text:
            text = text.replace(old, new, 1)
            notes.append(label)
        else:
            notes.append(f"{label} MISSING")
    path.write_text(text, encoding="utf-8")
    return notes


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <raylib_src_dir>", file=sys.stderr)
        return 2

    root = pathlib.Path(sys.argv[1])
    rcore = root / "src" / "platforms" / "rcore_desktop_rgfw.c"
    rgfw = root / "src" / "external" / "RGFW" / "RGFW.h"
    if not rcore.is_file() or not rgfw.is_file():
        print(f"error: need {rcore} and {rgfw}", file=sys.stderr)
        return 1

    n1 = patch_rcore(rcore)
    n2 = patch_rgfw(rgfw)
    print(f"sw-present: rcore={n1}; rgfw={n2}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
