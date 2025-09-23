// VitaVideoRenderer.cpp - Implementación del renderer unificado
#include "VitaVideoRenderer.hpp"
#include "legacy/modules/vita_globals.h"
#include <borealis/core/application.hpp>
#include <borealis/extern/nanovg/nanovg.h>
#include <vita2d.h>

VitaVideoRenderer& VitaVideoRenderer::instance() { static VitaVideoRenderer inst; return inst; }

void VitaVideoRenderer::invalidate() {
	if (video_nvg_image_id != 0) {
		if (auto vg = brls::Application::getNVGContext()) nvgDeleteImage(vg, video_nvg_image_id);
		video_nvg_image_id = 0; video_nvg_image_ready = false; video_nvg_frame_dirty = false;
	}
}

void VitaVideoRenderer::draw(NVGcontext* vg, float x, float y, float w, float h, float alpha) {
	if (!vg) return;
	// Subir frame pendiente desde staging
	if (video_nvg_frame_dirty && decoder_out_rgba) {
		size_t frame_bytes = (size_t)image_scaling.texture_width * (size_t)image_scaling.texture_height * 4;
		if (decoder_out_rgba_size >= frame_bytes) {
			if (video_nvg_image_id == 0) {
				video_nvg_image_id = nvgCreateImageRGBA(vg, image_scaling.texture_width, image_scaling.texture_height, NVG_IMAGE_STREAMING, decoder_out_rgba);
			} else {
				nvgUpdateImage(vg, video_nvg_image_id, decoder_out_rgba);
			}
			video_nvg_image_ready = true;
		}
		video_nvg_frame_dirty = false;
	}
	// Snapshot inicial si no hay imagen todavía
	if (!video_nvg_image_ready && video_nvg_image_id == 0 && FRAME_FRONT()) {
		uint8_t* front = (uint8_t*)vita2d_texture_get_datap(FRAME_FRONT());
		if (front) {
			size_t frame_bytes = (size_t)image_scaling.texture_width * (size_t)image_scaling.texture_height * 4;
			if (decoder_out_rgba && decoder_out_rgba_size >= frame_bytes) {
				memcpy(decoder_out_rgba, front, frame_bytes);
				video_nvg_frame_dirty = true; // se procesará en siguiente llamada
			}
		}
	}
	if (!video_nvg_image_ready || video_nvg_image_id == 0) return;
	float drawW = w, drawH = h, drawX = x, drawY = y;
	if (!video_fullscreen_stretch && image_scaling.enabled) {
		// Letterbox (fit) conservando aspect original
		drawW = (float)image_scaling.display_width;
		drawH = (float)image_scaling.display_height;
		drawX = (w - drawW) * 0.5f;
		drawY = (h - drawH) * 0.5f;
	} else if (video_fullscreen_stretch) {
		// Estirar a pantalla completa sin mantener aspect (solicitud usuario)
		drawW = w;
		drawH = h;
		drawX = x;
		drawY = y;
	}
	nvgBeginPath(vg);
	NVGpaint paint = nvgImagePattern(vg, drawX, drawY, drawW, drawH, 0, video_nvg_image_id, alpha);
	nvgRect(vg, drawX, drawY, drawW, drawH);
	nvgFillPaint(vg, paint);
	nvgFill(vg);

	// Actualizar estadísticas de presentación
	g_stats.frames_presented++;
	static uint32_t window_present = 0;
	window_present++;
	uint64_t now_ms = vita_monotonic_ms();
	if (last_fps_window_ms == 0) {
		last_fps_window_ms = now_ms;
	}
	uint64_t elapsed = now_ms - last_fps_window_ms;
	if (elapsed >= 1000) { // ventana ~1s
		g_stats.current_fps = (uint32_t)((uint64_t)window_present * 1000ULL / (elapsed ? elapsed : 1ULL));
		last_fps_window_ms = now_ms;
		window_present = 0;
	}
}
