/* Host-compile stubs for ccc --emit-c-only of span_kernels.ccs.
 * Included only under #ifdef CC_PARSER_MODE (inert when pasted into rlsw.h). */
#ifndef SW_SPAN_KERNELS_HOST_H
#define SW_SPAN_KERNELS_HOST_H

#include <stdint.h>
#include <math.h>

typedef struct sw_vertex {
    float position[4];
    float color[4];
    float texcoord[2];
} sw_vertex_t;

typedef struct {
    void *pixels;
    int width, height, wMinus1, hMinus1;
} sw_texture_t;

typedef struct {
    sw_texture_t *colorBuffer;
    sw_texture_t *depthBuffer;
    sw_texture_t *boundTexture;
    int adaptiveAffine;
} sw_ctx_stub_t;

extern sw_ctx_stub_t RLSW;

typedef struct {
    int valid, baryFix, xplane, yplane;
    int32_t dbudx, dbudy, dbvdx, dbvdy;
    float buf, bvf, dbudxf, dbudyf, dbvdxf, dbvdyf;
    sw_vertex_t p1, p2, p3;
} sw_tri_planes_t;

typedef struct sw_fill_ctx {
    sw_tri_planes_t *planes;
    sw_texture_t *tex;
    int clipOn, cx0, cy0, cx1, cy1;
} sw_fill_ctx_t;

static inline sw_texture_t *sw_fill_tex(const sw_fill_ctx_t *fc)
{
    return fc->tex ? fc->tex : RLSW.boundTexture;
}

#define SW_FRAMEBUFFER_COLOR_SIZE 4
#define SW_FRAMEBUFFER_DEPTH_SIZE 4
#define SW_FRAMEBUFFER_DEPTH_GET(p, o) (*(const float *)(p))
#define SW_FRAMEBUFFER_DEPTH_SET(p, d, o) (*(float *)(p) = (d))

static inline void sw_tri_bary_at(const sw_tri_planes_t *pl, int x, int y, int64_t *bu, int64_t *bv)
{
    (void)pl; (void)x; (void)y; *bu = 0; *bv = 0;
}
static inline float sw_tri_dwdx(const sw_tri_planes_t *pl) { (void)pl; return 0.0f; }
static inline void sw_tri_attrib(const sw_tri_planes_t *pl, int64_t bu, int64_t bv, float *w, float *u, float *v, float c[4])
{
    (void)pl; (void)bu; (void)bv; *w = 1; *u = 0; *v = 0; c[0] = c[1] = c[2] = c[3] = 1;
}
static inline void sw_tri_attrib_f(const sw_tri_planes_t *pl, float fu, float fv, float *w, float *u, float *v, float c[4])
{
    (void)pl; (void)fu; (void)fv; *w = 1; *u = 0; *v = 0; c[0] = c[1] = c[2] = c[3] = 1;
}
static inline int32_t sw_to_8_24(float x) { return (int32_t)(x * 16777216.0f); }
static inline int32_t sw_to_16_16(float x) { return (int32_t)(x * 65536.0f); }

static inline uint32_t sw_modulate_rgba8(uint32_t texel, int32_t cr, int32_t cg, int32_t cb, int32_t ca)
{
    (void)cr; (void)cg; (void)cb; (void)ca;
    return texel;
}
static inline uint32_t sw_rgba8_from_color24(int32_t cr, int32_t cg, int32_t cb, int32_t ca)
{
    (void)cr; (void)cg; (void)cb; (void)ca;
    return 0xFFFFFFFFu;
}
static inline uint32_t sw_blend_src_over_rgba8(uint32_t dst, uint32_t src)
{
    (void)dst;
    return src;
}
static inline uint32_t sw_texel_nearest_pot(const uint32_t *t, int tw, int um, int vm, int32_t iu, int32_t iv)
{
    (void)tw; (void)um; (void)vm; (void)iu; (void)iv;
    return t[0];
}
static inline uint32_t sw_texel_nearest_npot(const uint32_t *t, int tw, int th, int32_t iu, int32_t iv)
{
    (void)tw; (void)th; (void)iu; (void)iv;
    return t[0];
}
static inline uint32_t sw_texel_nearest_clamp(const uint32_t *t, int tw, int th, int32_t iu, int32_t iv)
{
    (void)tw; (void)th; (void)iu; (void)iv;
    return t[0];
}
static inline uint32_t sw_texel_bilerp_pot(const uint32_t *t, int tw, int um, int vm, int32_t iu, int32_t iv)
{
    (void)tw; (void)um; (void)vm; (void)iu; (void)iv;
    return t[0];
}
static inline uint32_t sw_texel_bilerp_npot(const uint32_t *t, int tw, int th, int32_t iu, int32_t iv)
{
    (void)tw; (void)th; (void)iu; (void)iv;
    return t[0];
}
static inline uint32_t sw_texel_bilerp_clamp(const uint32_t *t, int tw, int th, int32_t iu, int32_t iv)
{
    (void)tw; (void)th; (void)iu; (void)iv;
    return t[0];
}

/* Satisfy linker for host typecheck of emit (not shipped). */
sw_ctx_stub_t RLSW;

#endif
