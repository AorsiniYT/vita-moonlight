#include "vita_globals.h"
#ifdef BOREALIS_USE_GXM
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/display.h>
#include <psp2/videodec.h>
#include <vita2d.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <borealis/core/application.hpp>
#include <borealis/extern/nanovg/nanovg.h>
#include "../../../network/NetworkOptimizations.h"

// Definir constantes que pueden faltar
#ifndef SCE_VIDEODEC_TYPE_HW_AVCDEC
#define SCE_VIDEODEC_TYPE_HW_AVCDEC ((SceVideodecType)0x1001)
#endif

// Tiempo monotónico en ms
uint64_t vita_monotonic_ms() {
	return sceKernelGetSystemTimeWide() / 1000ULL;
}

// Thread pacer (limita FPS y setea need_drop)
int vita_pacer_thread_main(SceSize args, void* argp) {
	(void)args; (void)argp;
	VITA_DEBUG_LOG("[Video][PACER] thread iniciado");
	uint64_t last_50_tick = vita_monotonic_ms();
	uint64_t last_500_tick = last_50_tick;
	// Sincronizar target fps con configuración actual (curr_fps[1] se actualiza externamente)
	vita_netopt_set_target_fps(curr_fps[1] > 0 ? curr_fps[1] : 60);
	while (active_pacer_thread) {
		sceKernelDelayThread(5 * 1000); // sleep corto (~5ms) para granularidad sin busy-wait
		uint64_t now = vita_monotonic_ms();

		// Tick granular 50ms (loss window / conexión)
		if (now - last_50_tick >= 50) {
			vita_netopt_tick_50ms();
			last_50_tick = now;
		}
		// Tick backoff general (reusa anterior función)
		if (now - last_500_tick >= 500) {
			vita_netopt_tick();
			last_500_tick = now;
		}

		// Consumir drop budget adaptativo y cargar en need_drop (legacy variable) para compatibilidad
		unsigned drops = vita_netopt_consume_drop_budget();
		if (drops) {
			need_drop += (int)drops;
			if (need_drop < 0) need_drop = 0;
		}
	}
	VITA_DEBUG_LOG("[Video][PACER] thread saliendo");
	return 0;
}

extern "C" void vita_cleanup() {
	VITA_DEBUG_LOG("[Video] vita_cleanup llamado");
	active_pacer_thread = false;
	if (pacer_thread >= 0) { pacer_thread = -1; }
	if (decoder) { sceAvcdecDeleteDecoder(decoder); free(decoder); decoder = NULL; }
	if (decoderblock >= 0) { sceKernelFreeMemBlock(decoderblock); decoderblock = -1; }
	if (init) {
		VITA_DEBUG_LOG("[Video] Terminando librería AVC (sceVideodecTermLibrary)...");
		sceVideodecTermLibrary(SCE_VIDEODEC_TYPE_HW_AVCDEC);
		free(init); init = NULL;
	}
	if (decoder_info) { free(decoder_info); decoder_info = NULL; }
	for (int i=0;i<2;i++) { if (frame_textures[i]) { vita2d_free_texture(frame_textures[i]); frame_textures[i]=NULL; } }
	if (decoder_buffer) { free(decoder_buffer); decoder_buffer = NULL; decoder_buffer_size = 0; }
	if (decoder_yuv_raw) { free(decoder_yuv_raw); decoder_yuv_raw=NULL; decoder_yuv_buffer=NULL; decoder_yuv_buffer_size=0; }
	if (decoder_rgba_buffer) { decoder_rgba_buffer = nullptr; }
	if (decoder_out_rgba) { free(decoder_out_rgba); decoder_out_rgba = NULL; decoder_out_rgba_size = 0; }
	if (decoder_output_phys_block >= 0) {
		sceKernelFreeMemBlock(decoder_output_phys_block);
		decoder_output_phys_block = -1;
		decoder_output_phys_ptr = nullptr;
		decoder_output_phys_size = 0;
	}
	if (g_sps_ctx) { delete g_sps_ctx; g_sps_ctx = NULL; }
	if (video_nvg_image_id != 0) {
		NVGcontext* vg = brls::Application::getNVGContext();
		if (vg) nvgDeleteImage(vg, video_nvg_image_id);
		video_nvg_image_id = 0;
		video_nvg_image_ready = false;
		video_nvg_frame_dirty = false;
	}
	video_status = VITA_VIDEO_NOT_INIT;
	VITA_DEBUG_LOG("[Video] cleanup completado");
}

// Stubs (placeholder)
void vitavideo_draw_fps() {}
void vitavideo_draw_indicators() {}

#endif // BOREALIS_USE_GXM