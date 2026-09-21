/* Fast macOS software present for raylib RGFW (part of rlsw-cc soft present).
 *
 * Copies the rlsw color buffer (GL bottom-up RGBA) into a triple-buffered
 * top-down staging surface, then wraps that in CGImage for CALayer.
 *
 * Zero-copy into the live FB races Core Animation (clear/redraw while CA still
 * reads) → flicker / missing tris / HUD flash. Staging avoids that without
 * paying swReadPixels BGRA scramble.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <CoreGraphics/CoreGraphics.h>
#include <objc/message.h>
#include <objc/runtime.h>

typedef struct {
    int w, h;
    CGColorSpaceRef colorSpace;
    uint8_t *stage[3];   /* top-down RGBA; owned. Triple so CA can lag 1–2 frames. */
    size_t stageBytes;
    int writeIdx;        /* next buffer to fill */
    int windowCSApplied;
} PresentState;

static PresentState g_present;

static void present_free_stages(void)
{
    for (int i = 0; i < 3; i++) {
        free(g_present.stage[i]);
        g_present.stage[i] = NULL;
    }
    g_present.stageBytes = 0;
    g_present.w = g_present.h = 0;
}

static CGColorSpaceRef present_copy_dest_colorspace(void)
{
    CGDirectDisplayID did = CGMainDisplayID();
    CGColorSpaceRef cs = CGDisplayCopyColorSpace(did);
    if (cs) return cs;
    cs = CGColorSpaceCreateDeviceRGB();
    if (cs) return cs;
    return CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
}

static void present_apply_window_colorspace(id view, CGColorSpaceRef cs)
{
    if (g_present.windowCSApplied || !view || !cs) return;

    id window = ((id (*)(id, SEL))objc_msgSend)(view, sel_getUid("window"));
    if (!window) return;

    Class NSColorSpace = objc_getClass("NSColorSpace");
    if (!NSColorSpace) return;

    id nscs = ((id (*)(id, SEL, CGColorSpaceRef))objc_msgSend)(
        ((id (*)(Class, SEL))objc_msgSend)(NSColorSpace, sel_getUid("alloc")),
        sel_getUid("initWithCGColorSpace:"),
        cs);
    if (!nscs) return;

    ((void (*)(id, SEL, id))objc_msgSend)(window, sel_getUid("setColorSpace:"), nscs);
    ((void (*)(id, SEL))objc_msgSend)(nscs, sel_getUid("release"));
    g_present.windowCSApplied = 1;
}

static int present_ensure(int w, int h)
{
    size_t nbytes = (size_t)w * (size_t)h * 4u;
    if (g_present.w == w && g_present.h == h && g_present.stage[0] && g_present.stage[1]
        && g_present.stage[2] && g_present.colorSpace)
        return 1;

    present_free_stages();
    if (g_present.colorSpace) {
        CGColorSpaceRelease(g_present.colorSpace);
        g_present.colorSpace = NULL;
    }

    for (int i = 0; i < 3; i++) {
        g_present.stage[i] = (uint8_t *)malloc(nbytes);
        if (!g_present.stage[i]) {
            present_free_stages();
            return 0;
        }
    }
    g_present.stageBytes = nbytes;
    g_present.w = w;
    g_present.h = h;
    g_present.writeIdx = 0;
    g_present.colorSpace = present_copy_dest_colorspace();
    return g_present.colorSpace != NULL;
}

/* GL bottom-up → top-down RGBA into dst (row-major). */
static void present_blit_flip(uint8_t *dst, const uint8_t *src, int w, int h)
{
    size_t stride = (size_t)w * 4u;
    for (int y = 0; y < h; y++)
        memcpy(dst + (size_t)y * stride, src + (size_t)(h - 1 - y) * stride, stride);
}

void RayrenderMacOSPresent(void *nsview, void *pixels, int width, int height)
{
    if (!nsview || !pixels || width <= 0 || height <= 0) return;
    if (!present_ensure(width, height)) return;

    present_apply_window_colorspace((id)nsview, g_present.colorSpace);

    int wi = g_present.writeIdx;
    present_blit_flip(g_present.stage[wi], (const uint8_t *)pixels, width, height);

    /* CreateWithData does not copy; CGImage keeps a pointer into stage[wi].
     * Triple-buffer so CA can still composite an older frame while we fill. */
    CGDataProviderRef provider = CGDataProviderCreateWithData(
        NULL, g_present.stage[wi], g_present.stageBytes, NULL);
    if (!provider) return;

    CGBitmapInfo bitmapInfo =
        (CGBitmapInfo)kCGImageAlphaPremultipliedLast | (CGBitmapInfo)kCGBitmapByteOrder32Big;

    CGImageRef image = CGImageCreate(
        (size_t)width,
        (size_t)height,
        8,
        32,
        (size_t)width * 4u,
        g_present.colorSpace,
        bitmapInfo,
        provider,
        NULL,
        false,
        kCGRenderingIntentAbsoluteColorimetric);
    CGDataProviderRelease(provider);
    if (!image) return;

    id layer = ((id (*)(id, SEL))objc_msgSend)((id)nsview, sel_getUid("layer"));
    if (layer)
        ((void (*)(id, SEL, id))objc_msgSend)(layer, sel_getUid("setContents:"), (id)image);

    CGImageRelease(image);
    g_present.writeIdx = (wi + 1) % 3;
}
