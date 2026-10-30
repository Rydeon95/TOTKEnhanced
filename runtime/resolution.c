// Game resolution: the size the game composes its frame in (its output: canvas, UI and swapchain) and the size of its
// 3D scene, above the shipped 1920x1080 and 1600x900. The game does the rendering itself, as with the community
// resolution mods (UltraCam, Visual Fixes): the patch sets output and scene (recomp/patches-v121.txt,
// patches-v143.txt; MacNativeGames games/totk/runtime/game_patches.c and game_patches_v143.c, MIT, Copyright (c) 2026
// MacNativeGames contributors) make the game read both sizes from the patch page, and the texture heap that holds the
// output- and scene-sized render targets grows with them.
//
// Settings, read at start: TOTK_OUTPUT_RES = <w>x<h> (default 1920x1080, the game's own), TOTK_RENDER_SCALE = the
// scene's size relative to the output: "game" (default, 5/6, the game's 1600x900 at 1080p) or a percentage; above 100
// the game supersamples its scene, which smooths edges (anti-aliasing) at the cost of GPU time. The game's own final
// pass (FSR 1 EASU) scales the scene to the output; the present scales the output to the window (vk_fsr.c).
//
// Patch page: +16 the scene size {w, h} as floats, +32 the output size and +48 the swapchain size as {w, h} in one
// 64-bit word, +40 the renderer (1.2.1) or display configuration (1.4.3) the patched code stores there.
// 1.4.3 scales the scene it reads by output width / 1920 (a 2560x1440 output renders its 1600x900 scene at 2128x1200;
// MacNativeGames measured the same), so its page holds the wanted size divided by that factor and the game's 5/6 needs
// no scene patch. 1.2.1 renders the size it reads (1600x900 on a 2560x1440 output): its page holds the wanted size, and
// the scene set is on whenever the output differs from 1920x1080.
//
// Heap: agl's DynamicTextureAllocator is one fixed heap, "DynamicTextureAllocatorMem" in
// System/HeapSize/Product.Nin_NX_NVN.HeapDef.byml.zs (113664 KiB, carved from "Graphics", 201728 KiB). At 2560x1440
// the shipped size runs out at the title screen. Both heaps grow by the same amount: the shipped size times the output's
// pixel ratio above 1920x1080 plus the scene's above 1600x900 (MacNativeGames' measurement: a 3440x1440 canvas with a
// 4620x1935 scene crashed at +258 MiB and loaded at +400 MiB; this formula gives +750 MiB there). romfs.c serves the
// rewritten table (res_heapdef_build) in place of the original; memory.c adds the amount to the application pool, so
// no other heap shrinks. Without the rewritten table (a RomFS that lacks it, a format this code does not know) the
// patch sets stay off and the game keeps its own sizes.
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zstd.h>

#include "cfg.h"
#include "host.h"

#define BASE_W 1920
#define BASE_H 1080
#define SCENE_W 1600
#define SCENE_H 900
#define DTA_KIB 113664u      // DynamicTextureAllocatorMem, shipped
#define MAX_OUTPUT_W 7680
#define MAX_OUTPUT_H 4320
// Let the scene reach every accepted output size; the upstream patch-tool cap of 4096 is not a Vulkan limit.

static struct {
    int decided;
    unsigned ow, oh, sw, sh;   // output and scene
    int scene_set;             // the scene differs from the game's 5/6 of the output
    int heapdef_ok;            // romfs.c serves the grown HeapDef
    int on;                    // res_init switched the patch sets on
} R;

static int scene_dimensions(unsigned ow, unsigned oh, double scale, unsigned *sw, unsigned *sh) {
    double want_w = ow * scale, want_h = oh * scale;
    double fit = fmin(1.0, fmin(MAX_OUTPUT_W / want_w, MAX_OUTPUT_H / want_h));
    *sw = (unsigned)(want_w * fit + 0.5);
    *sh = (unsigned)(want_h * fit + 0.5);
    return fit < 1.0;
}

static void decide(void) {
    if (R.decided) return;
    R.decided = 1;
    R.ow = BASE_W;
    R.oh = BASE_H;
    const char *o = cfg_value("TOTK_OUTPUT_RES");
    unsigned w = 0, h = 0;
    char tail;
    if (sscanf(o, "%ux%u%c", &w, &h, &tail) == 2 && w >= 1280 && h >= 720 && w <= MAX_OUTPUT_W && h <= MAX_OUTPUT_H) {
        R.ow = w & ~1u;
        R.oh = h & ~1u;
    } else {
        rt_log("resolution: TOTK_OUTPUT_RES=%s is not <width>x<height> in 1280x720..7680x4320; 1920x1080", o);
    }
    const char *s = cfg_value("TOTK_RENDER_SCALE");
    double k = 5.0 / 6.0;
    if (strcmp(s, "game")) {
        char *end;
        double pct = strtod(s, &end);
        if (end != s && !*end && pct >= 25 && pct <= 400) {
            k = pct / 100.0;
            R.scene_set = 1;
        } else {
            rt_log("resolution: TOTK_RENDER_SCALE=%s is not game or a percentage 25..400; game", s);
        }
    }
    if (scene_dimensions(R.ow, R.oh, k, &R.sw, &R.sh)) R.scene_set = 1;
}

static int bigger(void) {
    decide();
    return R.ow != BASE_W || R.oh != BASE_H || R.scene_set;
}

static int ui_safe_area_for(unsigned ow, unsigned oh, double safe[4], double *x_scale) {
    safe[0] = 0;
    safe[1] = 0;
    safe[2] = ow;
    safe[3] = oh;
    *x_scale = 1.0;
    if ((uint64_t)ow * 9 <= (uint64_t)oh * 16) return 0;
    safe[2] = (double)oh * (16.0 / 9.0);
    safe[0] = ((double)ow - safe[2]) * 0.5;
    *x_scale = (double)ow / safe[2];
    return 1;
}

int res_ui_safe_area(unsigned output[2], double safe[4], double *x_scale) {
    decide();
    output[0] = R.on ? R.ow : BASE_W;
    output[1] = R.on ? R.oh : BASE_H;
    int wide = ui_safe_area_for(output[0], output[1], safe, x_scale);
    return R.on && wide;
}

uint64_t res_heap_extra_kib(void) {
    if (!bigger()) return 0;
    double grow = fmax(0.0, (double)R.ow * R.oh / (BASE_W * BASE_H) - 1.0) +
                  fmax(0.0, (double)R.sw * R.sh / (SCENE_W * SCENE_H) - 1.0);
    if (grow <= 0) return 0;
    return (uint64_t)ceil(DTA_KIB * (1.0 + grow) / 1024.0) * 1024 - DTA_KIB;
}

uint64_t res_heap_extra_bytes(void) { return ((res_heap_extra_kib() << 10) + 0x1fffff) & ~0x1fffffull; }

int res_selftest(void) {
    static const unsigned modes[][2] = {{1920, 1080}, {2560, 1440}, {3440, 1440}, {3840, 2160}, {5120, 2160}};
    for (unsigned i = 0; i < sizeof modes / sizeof *modes; i++) {
        unsigned sw = 0, sh = 0;
        if (scene_dimensions(modes[i][0], modes[i][1], 1.0, &sw, &sh) || sw != modes[i][0] || sh != modes[i][1]) {
            rt_log("resolution selftest: %ux%u at 100%% produced scene %ux%u", modes[i][0], modes[i][1], sw, sh);
            return 0;
        }
    }
    static const struct { unsigned ow, oh; double x, w, sx; } ui_modes[] = {
        {1920, 1080, 0, 1920, 1}, {2560, 1440, 0, 2560, 1},
        {3440, 1440, 440, 2560, 1.34375}, {3840, 2160, 0, 3840, 1},
        {5120, 2160, 640, 3840, 4.0 / 3.0},
    };
    for (unsigned i = 0; i < sizeof ui_modes / sizeof *ui_modes; i++) {
        double safe[4], sx;
        int wide = ui_safe_area_for(ui_modes[i].ow, ui_modes[i].oh, safe, &sx);
        if (fabs(safe[0] - ui_modes[i].x) > 1e-9 || safe[1] != 0 || fabs(safe[2] - ui_modes[i].w) > 1e-9 ||
            safe[3] != ui_modes[i].oh || fabs(sx - ui_modes[i].sx) > 1e-9 || wide != (ui_modes[i].sx > 1.0)) {
            rt_log("resolution selftest: UI safe area for %ux%u was x=%g w=%g scale=%g", ui_modes[i].ow, ui_modes[i].oh,
                   safe[0], safe[2], sx);
            return 0;
        }
    }
    decide();
    if (!strcmp(cfg_value("TOTK_RENDER_SCALE"), "100") && (R.sw != R.ow || R.sh != R.oh)) {
        rt_log("resolution selftest: configured %ux%u at 100%% produced scene %ux%u", R.ow, R.oh, R.sw, R.sh);
        return 0;
    }
    uint64_t extra_kib = res_heap_extra_kib();
    if (extra_kib > (uint64_t)INT32_MAX - 201728u || extra_kib > (UINT64_MAX - 0x1fffff) >> 10) {
        rt_log("resolution selftest: texture heap growth overflows HeapDef or application-pool size");
        return 0;
    }
    uint64_t extra_bytes = res_heap_extra_bytes();
    if (extra_bytes < (extra_kib << 10) || (extra_bytes & 0x1fffff)) {
        rt_log("resolution selftest: application-pool growth is not safely aligned");
        return 0;
    }
    return 1;
}

// ---- the HeapDef table. BYML version 2..7, little endian ("YB"): a hash node is {u8 0xC1, u24 count} and count
// entries {u24 key index, u8 type, u32 value}; an int (type 0xD1) is stored in the entry, so a size changes in place.
typedef struct { uint8_t *p; size_t n; uint32_t keys; } Byml;

static int u32_at(const Byml *b, size_t off, uint32_t *v) {
    if (off + 4 > b->n) return 0;
    memcpy(v, b->p + off, 4);
    return 1;
}

static const char *key_name(const Byml *b, uint32_t k) {
    uint32_t head, at;
    if (!u32_at(b, b->keys, &head) || (head & 0xff) != 0xc2 || k >= head >> 8 || !u32_at(b, b->keys + 4 + 4 * k, &at) ||
        b->keys + at >= b->n || !memchr(b->p + b->keys + at, 0, b->n - b->keys - at))
        return NULL;
    return (const char *)b->p + b->keys + at;
}

// The offset of the value of hash node `node`'s entry `name`, which must have type `type`; 0 when there is none.
static size_t field(const Byml *b, uint32_t node, const char *name, uint8_t type) {
    uint32_t head;
    if (!u32_at(b, node, &head) || (head & 0xff) != 0xc1) return 0;
    for (uint32_t i = 0; i < head >> 8; i++) {
        uint32_t e;
        if (!u32_at(b, node + 4 + 8 * (size_t)i, &e)) return 0;
        const char *k = key_name(b, e & 0xffffff);
        if (k && !strcmp(k, name)) return e >> 24 == type ? node + 8 + 8 * (size_t)i : 0;
    }
    return 0;
}

static size_t heap_size_field(const Byml *b, uint32_t heaps, const char *heap) {
    size_t at = field(b, heaps, heap, 0xc1);
    uint32_t node;
    return at && u32_at(b, at, &node) ? field(b, node, "Size", 0xd1) : 0;
}

int res_heapdef_build(const void *in, size_t n, void **out, size_t *out_n) {
    uint64_t extra = res_heap_extra_kib();
    *out = NULL;
    if (!extra) return 0;
    if (ZSTD_getDictID_fromFrame(in, n)) {
        rt_log("resolution: the HeapDef table needs a zstd dictionary; the game keeps its own resolution");
        return 0;
    }
    unsigned long long raw = ZSTD_getFrameContentSize(in, n);
    if (raw == ZSTD_CONTENTSIZE_UNKNOWN || raw == ZSTD_CONTENTSIZE_ERROR || raw > (64u << 20)) {
        rt_log("resolution: the HeapDef table is not a zstd frame with a size; the game keeps its own resolution");
        return 0;
    }
    Byml b = {malloc((size_t)raw), (size_t)raw, 0};
    if (!b.p) return 0;
    size_t got = ZSTD_decompress(b.p, b.n, in, n);
    uint32_t root, heaps = 0;
    uint16_t ver = 0;
    if (!ZSTD_isError(got) && got == b.n && b.n >= 16) memcpy(&ver, b.p + 2, 2);
    size_t heaps_at = 0, dta_at = 0, gfx_at = 0;
    if (ver >= 2 && ver <= 7 && !memcmp(b.p, "YB", 2) && u32_at(&b, 4, &b.keys) && u32_at(&b, 12, &root) &&
        (heaps_at = field(&b, root, "Heaps", 0xc1)) && u32_at(&b, heaps_at, &heaps)) {
        dta_at = heap_size_field(&b, heaps, "DynamicTextureAllocatorMem");
        gfx_at = heap_size_field(&b, heaps, "Graphics");
    }
    if (!dta_at || !gfx_at) {
        rt_log("resolution: the HeapDef table has no DynamicTextureAllocatorMem / Graphics sizes; the game keeps its own "
               "resolution");
        free(b.p);
        return 0;
    }
    int32_t dta, gfx;
    memcpy(&dta, b.p + dta_at, 4);
    memcpy(&gfx, b.p + gfx_at, 4);
    int32_t dta2 = dta + (int32_t)extra, gfx2 = gfx + (int32_t)extra;
    memcpy(b.p + dta_at, &dta2, 4);
    memcpy(b.p + gfx_at, &gfx2, 4);
    size_t cap = ZSTD_compressBound(b.n);
    void *z = malloc(cap);
    size_t zn = z ? ZSTD_compress(z, cap, b.p, b.n, 9) : 0;
    free(b.p);
    if (!z || ZSTD_isError(zn)) {
        free(z);
        return 0;
    }
    *out = z;
    *out_n = zn;
    rt_log("resolution: HeapDef DynamicTextureAllocatorMem %d -> %d KiB, Graphics %d -> %d KiB", dta, dta2, gfx, gfx2);
    return 1;
}

void res_heapdef_served(void) { R.heapdef_ok = 1; }

// ---- start: the patch page and the sets
static uint64_t pack(unsigned w, unsigned h) { return (uint64_t)h << 32 | w; }

void res_init(void) {
    if (!bigger()) {
        rt_log("resolution: output %ux%u, aspect %.6f, ultrawide off (the game's own scene 1600x900)", BASE_W, BASE_H,
               (double)BASE_W / BASE_H);
        rt_log("ultrawide: output %ux%u, aspect %.6f, correction disabled", BASE_W, BASE_H, (double)BASE_W / BASE_H);
        rt_log("ui: output %ux%u, safe area %ux%u +0x0", BASE_W, BASE_H, BASE_W, BASE_H);
        return;
    }
    int output = rt_patch_set("output"), scene = rt_patch_set("scene");
    if (output < 0 || scene < 0) {
        rt_log("resolution: the generated C has no output and scene patch sets; the game keeps its own resolution");
        rt_log("ultrawide: correction disabled (resolution patch sets unavailable)");
        rt_log("ui: safe-area correction disabled (resolution patch sets unavailable)");
        return;
    }
    fs_init();   // romfs.c serves the grown HeapDef from here on (res_heapdef_served)
    if (res_heap_extra_kib() && !R.heapdef_ok) {   // sizes at or below the shipped ones fit the shipped heaps
        rt_log("resolution: no grown HeapDef table; the game keeps its own resolution");
        rt_log("ultrawide: correction disabled (resolution heap unavailable)");
        rt_log("ui: safe-area correction disabled (resolution heap unavailable)");
        return;
    }
    uint8_t *page = rt_patch_page();
    double k = RT_V143 ? (double)R.ow / BASE_W : 1.0;   // 1.4.3's own scaling of the scene with the output width
    int scene_on = R.scene_set || (!RT_V143 && (R.ow != BASE_W || R.oh != BASE_H));
    float sc[2] = {(float)(R.sw / k), (float)(R.sh / k)};
    float aspect = (float)R.ow / (float)R.oh;
    uint64_t out = pack(R.ow, R.oh), none = 0;
    memcpy(page + 16, sc, sizeof sc);
    memcpy(page + 32, &out, 8);
    memcpy(page + 40, &none, 8);
    memcpy(page + 48, &out, 8);
    memcpy(page + 56, &aspect, sizeof aspect);
    rt_patch_switch(output, R.ow != BASE_W || R.oh != BASE_H);
    rt_patch_switch(scene, scene_on);
    int ultrawide = rt_patch_set("ultrawide");
    int ultrawide_on = ultrawide >= 0 && R.ow * 9u != R.oh * 16u;
    if (ultrawide >= 0) rt_patch_switch(ultrawide, ultrawide_on);
    R.on = 1;
    rt_log("resolution: output %ux%u, aspect %.6f, ultrawide %s, scene %ux%u%s; texture heap +%llu KiB", R.ow, R.oh,
           (double)R.ow / R.oh, ultrawide_on ? "enabled" : "off", R.sw, R.sh,
           scene_on ? "" : " (the game's own 5/6 of the output)", (unsigned long long)res_heap_extra_kib());
    rt_log("ultrawide: output %ux%u, aspect %.6f, correction %s", R.ow, R.oh, (double)R.ow / R.oh,
           ultrawide_on ? "enabled" : "disabled");
    double safe[4], x_scale;
    ui_safe_area_for(R.ow, R.oh, safe, &x_scale);
    rt_log("ui: output %ux%u, safe area %.0fx%.0f +%.0fx%.0f (x squeeze %.6f%s)", R.ow, R.oh, safe[2], safe[3],
           safe[0], safe[1], x_scale, ultrawide_on ? ", correction enabled" : "");
}

void res_sizes(unsigned out[2], unsigned scene[2]) {
    decide();
    out[0] = R.on ? R.ow : BASE_W, out[1] = R.on ? R.oh : BASE_H;
    scene[0] = R.on ? R.sw : SCENE_W, scene[1] = R.on ? R.sh : SCENE_H;
}
