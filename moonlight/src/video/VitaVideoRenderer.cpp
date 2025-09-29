// VitaVideoRenderer.cpp - Versión mínima sin zero-copy ni VideoPlane
#include "VitaVideoRenderer.hpp"
#include "legacy/modules/vita_globals.h"
#include <vita2d.h>
#include <borealis/extern/nanovg/nanovg.h>
#include <borealis/extern/nanovg/nanovg_gxm.h>
#include <stdlib.h>
#include <string.h>

namespace {
    uint32_t s_presentWindowFrames = 0;
    uint64_t s_cachedStatsStart = 0;

    void update_present_stats() {
        uint64_t now = vita_monotonic_ms();
        if (stats_start_ms == 0) {
            stats_start_ms = now;
        }

        if (stats_start_ms != 0 && stats_start_ms != s_cachedStatsStart) {
            s_cachedStatsStart = stats_start_ms;
            s_presentWindowFrames = 0;
            last_fps_window_ms = stats_start_ms;
        }

        if (last_fps_window_ms == 0) {
            last_fps_window_ms = now;
        }

        s_presentWindowFrames++;

        if (stats_start_ms <= now) {
            g_stats.session_ms = now - stats_start_ms;
        }

        uint64_t elapsed = now - last_fps_window_ms;
        if (elapsed >= 1000) {
            if (elapsed == 0) elapsed = 1;
            g_stats.current_fps = (uint32_t)((uint64_t)s_presentWindowFrames * 1000ULL / elapsed);
            curr_fps[0] = g_stats.current_fps;
            last_fps_window_ms = now;
            s_presentWindowFrames = 0;
        }
    }
}

// Implementación

VitaVideoRenderer& VitaVideoRenderer::instance() { static VitaVideoRenderer inst; return inst; }

void VitaVideoRenderer::setFullscreenStretch(bool stretch) {
    fullscreenStretch = stretch;
    video_fullscreen_stretch = stretch;
}

void VitaVideoRenderer::draw(struct NVGcontext* vg, float viewportW, float viewportH, float alpha) {
    (void)vg; (void)alpha;
    draw(viewportW, viewportH);
}

void VitaVideoRenderer::draw(float viewportW, float viewportH) {
    if (!vita2d_inited) {
        static bool logged=false; if(!logged){ VITA_DEBUG_LOG("[Video][DRAW] vita2d no inicializado"); logged=true; }
        return;
    }
    if (g_stats.frames_decoded == 0) {
        static bool logged=false; if(!logged){ VITA_DEBUG_LOG("[Video][DRAW] primer frame aún no decodificado - skip"); logged=true; }
        return;
    }
    vita2d_texture* tex = FRAME_FRONT();
    if (!tex) {
        static bool logged=false; if(!logged){ VITA_DEBUG_LOG("[Video][DRAW] FRAME_FRONT null"); logged=true; }
        return;
    }
    static uint32_t drawCounter = 0;
    if (drawCounter < 120 || (drawCounter % 60) == 0) {
        unsigned stride = vita2d_texture_get_stride(tex);
        VITA_DEBUG_LOG("[Video][DRAW] frame=%u tex=%p stride=%u frontIdx=%d backIdx=%d", drawCounter, tex, stride, frame_front_idx, frame_back_idx);
    }
    drawCounter++;
    if (!image_scaling.enabled) return;
    int dw = fullscreenStretch ? (int)viewportW : image_scaling.display_width;
    int dh = fullscreenStretch ? (int)viewportH : image_scaling.display_height;
    int ox = fullscreenStretch ? 0 : image_scaling.offset_x;
    int oy = fullscreenStretch ? 0 : image_scaling.offset_y;
    if (dw <= 0 || dh <= 0) return;
    if (image_scaling.region_x2 <= image_scaling.region_x1 || image_scaling.region_y2 <= image_scaling.region_y1) return;
    if (image_scaling.texture_width == 0 || image_scaling.texture_height == 0) return;
    float scaleX = (float)dw / image_scaling.texture_width;
    float scaleY = (float)dh / image_scaling.texture_height;
    if (scaleX <= 0.f || scaleY <= 0.f) return;
    vita2d_draw_texture_tint_part_scale(
        tex,
        (float)ox, (float)oy,
        image_scaling.region_x1, image_scaling.region_y1,
        image_scaling.region_x2, image_scaling.region_y2,
        scaleX,
        scaleY,
        0xFFFFFFFF
    );
    g_stats.frames_presented++;
    update_present_stats();
}

void VitaVideoRenderer::drawNVG(NVGcontext* vg, float viewportW, float viewportH, float alpha) {
    if (!vg) { draw(viewportW, viewportH); return; }
    if (g_stats.frames_decoded == 0) return;
    const vita2d_texture* tex = FRAME_FRONT();
    if (!tex) return;
    const SceGxmTexture* gxmTex = &tex->gxm_tex;
    static uint32_t drawNvgCounter = 0;
    if (drawNvgCounter < 120 || (drawNvgCounter % 60) == 0) {
        VITA_DEBUG_LOG("[Video][DRAW NVG] frame=%u tex=%p gxm=%p", drawNvgCounter, tex, gxmTex);
    }
    drawNvgCounter++;
    if (!image_scaling.enabled) return;

    uint32_t texW = image_scaling.texture_width;
    uint32_t texH = image_scaling.texture_height;
    if (texW == 0 || texH == 0) return;

    if (nvgImageId < 0 || tex != currentTexture || (int)texW != storedW || (int)texH != storedH) {
        destroyImage(vg);
        int imageId = nvgxmCreateImageFromHandle(vg, const_cast<SceGxmTexture*>(gxmTex));
        if (imageId <= 0) {
            VITA_DEBUG_LOG("[Video][DRAW NVG][ERR] nvgxmCreateImageFromHandle fallo tex=%p", tex);
            return;
        }
        nvgImageId = imageId;
        currentTexture = tex;
        storedW = (int)texW;
        storedH = (int)texH;
        // VITA_DEBUG_LOG("[Video][DRAW NVG] creado imageId=%d tex=%p w=%u h=%u", nvgImageId, tex, texW, texH);
    }

    int dw = fullscreenStretch ? (int)viewportW : image_scaling.display_width;
    int dh = fullscreenStretch ? (int)viewportH : image_scaling.display_height;
    int ox = fullscreenStretch ? 0 : image_scaling.offset_x;
    int oy = fullscreenStretch ? 0 : image_scaling.offset_y;
    if (dw <= 0 || dh <= 0) return;

    NVGpaint paint = nvgImagePattern(vg, (float)ox, (float)oy, (float)dw, (float)dh, 0.0f, nvgImageId, alpha);
    nvgBeginPath(vg);
    nvgRect(vg, (float)ox, (float)oy, (float)dw, (float)dh);
    nvgFillPaint(vg, paint);
    nvgFill(vg);
    g_stats.frames_presented++;
    update_present_stats();
}

void VitaVideoRenderer::destroyImage(NVGcontext* vg) {
    if (nvgImageId >= 0 && vg) {
        nvgDeleteImage(vg, nvgImageId);
    }
    nvgImageId = -1;
    currentTexture = nullptr;
    storedW = storedH = 0;
}
