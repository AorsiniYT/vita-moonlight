#include "video/BorealisVideoView.hpp"
#include <borealis/core/frame_context.hpp>
#include <borealis/core/style.hpp>
#include <borealis/extern/nanovg/nanovg.h>
#include <borealis/core/application.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/extern/nanovg/nanovg_gxm.h>
#include <psp2/gxm.h>
#include <chrono>
#include "legacy/modules/vita_globals.h"
#include "video/VideoFrameHolder.hpp"

// Globals ya declarados en vita_globals.h (video_fullscreen_stretch, image_scaling)

static BorealisVideoView* g_instance = nullptr;

BorealisVideoView* BorealisVideoView::instance() { return g_instance; }

BorealisVideoView::BorealisVideoView() {
    g_instance = this;
}

BorealisVideoView::~BorealisVideoView() {
    // Imagen NVG se destruye posteriormente (destroyImage en draw o dtor)
}

void BorealisVideoView::destroyImage(NVGcontext* vg) {
    if (nvgImageId > 0 && vg) { nvgDeleteImage(vg, nvgImageId); }
    nvgImageId = -1; recreateImage=false; storedW=storedH=0;
    currentTexture = nullptr;
}

void BorealisVideoView::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
    (void)style; (void)ctx; (void)x; (void)y;
    if (!vg) return;

    // Extraer frame más reciente
    GxmFrame frame;
    bool has = VideoFrameHolder::instance().popLatest(frame);
    if (!has) {
        static uint32_t noFrameCounter = 0;
        if (noFrameCounter < 120 || (noFrameCounter % 60) == 0) {
            VITA_DEBUG_LOG("[VideoView] sin frame nuevo (count=%u)", noFrameCounter);
        }
        noFrameCounter++;
    }
    uint64_t now = vita_monotonic_ms();
    if (lastStatsMs == 0) lastStatsMs = now;

    if (has) {
        framesSeen++;
        uint32_t w = frame.w; uint32_t h = frame.h;
        if (w==0 || h==0 || !frame.texture || !frame.gxmTexture) {
            VITA_DEBUG_LOG("[VideoView][WARN] frame invalido tex=%p gxm=%p w=%u h=%u",
                frame.texture,
                frame.gxmTexture,
                w,
                h);
            return;
        }
        if (recreateImage || nvgImageId <=0 || w!=storedW || h!=storedH || frame.texture != currentTexture) {
            destroyImage(vg);
            nvgImageId = nvgxmCreateImageFromHandle(vg, const_cast<SceGxmTexture*>(frame.gxmTexture));
            if (nvgImageId <= 0) {
                brls::Logger::error("[VideoView][ERR] nvgxmCreateImageFromHandle fallo");
                return;
            }
            storedW = w; storedH = h; recreateImage=false;
            currentTexture = frame.texture;
            brls::Logger::info("[VideoView] imagen creada id={} w={} h={}", nvgImageId, w, h);
            VITA_DEBUG_LOG("[VideoView] nvgxmCreateImageFromHandle id=%d tex=%p gxm=%p w=%u h=%u",
                nvgImageId,
                frame.texture,
                frame.gxmTexture,
                w,
                h);
        }

        // Destino
        int dw, dh, ox, oy;
        if (video_fullscreen_stretch) { dw=(int)width; dh=(int)height; ox=0; oy=0; }
        else if (image_scaling.enabled) { dw=image_scaling.display_width; dh=image_scaling.display_height; ox=image_scaling.offset_x; oy=image_scaling.offset_y; }
        else { dw=(int)width; dh=(int)height; ox=0; oy=0; }
        if (dw>0 && dh>0) {
            NVGpaint p = nvgImagePattern(vg, (float)ox, (float)oy, (float)dw, (float)dh, 0.f, nvgImageId, 1.0f);
            nvgBeginPath(vg);
            nvgRect(vg, (float)ox, (float)oy, (float)dw, (float)dh);
            nvgFillPaint(vg, p);
            nvgFill(vg);
            framesDrawn++;
            VITA_DEBUG_LOG("[VideoView] draw rect (%d,%d %dx%d) imageId=%d", ox, oy, dw, dh, nvgImageId);
            lastFramePts = frame.ptsMs;
        }
    }

    if (now - lastStatsMs >= 1000) {
        brls::Logger::info("[VideoView][FPS] framesSeen={} drawn={}", framesSeen, framesDrawn);
        framesSeen = 0; framesDrawn = 0; lastStatsMs = now;
    }
}
