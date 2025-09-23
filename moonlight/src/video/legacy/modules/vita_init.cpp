#include "vita_globals.h"
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/display.h>
#include <psp2/videodec.h>
#include <vita2d.h>
#include <stdlib.h>
#include <memory>
#include <string.h>
#include <stdio.h>
#include <memory>
#include "../../../network/NetworkOptimizations.h"

// Definir constantes que pueden faltar
#ifndef SCE_VIDEODEC_TYPE_HW_AVCDEC
#define SCE_VIDEODEC_TYPE_HW_AVCDEC ((SceVideodecType)0x1001)
#endif

// Forward declarations
int vita_pacer_thread_main(SceSize args, void* argp);
extern "C" void vita_cleanup();
uint64_t vita_monotonic_ms();

extern "C" int vitavideo_setup(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags) {
    (void)videoFormat; (void)redrawRate; (void)context; (void)drFlags; // de momento no usados en esta ruta simplificada
    int ret = 0;
    // Registrar target FPS si es válido (>0)
    if (redrawRate > 0) {
        g_stats.target_fps = (uint32_t)redrawRate;
    }
    // Paso 1: framebuffer y buffers iniciales
    if (video_status == VITA_VIDEO_NOT_INIT) {
        decoder_buffer_size = DECODER_BUFFER_SIZE + AV_INPUT_BUFFER_PADDING_SIZE;
        decoder_buffer = (char*)malloc(decoder_buffer_size);
        if (!decoder_buffer) {
            VITA_DEBUG_LOG("[Video] Error: No hay memoria para decoder_buffer");
            ret = 0x80010001; goto cleanup; }

        vitavideo_update_scaling_settings(width, height); // define image_scaling

        for (int i=0;i<2;i++) {
            frame_textures[i] = vita2d_create_empty_texture_format(
                image_scaling.texture_width,
                image_scaling.texture_height,
                SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR);
            if (!frame_textures[i]) { VITA_DEBUG_LOG("[Video] Error: No hay memoria para frame_texture[%d]", i); ret = 0x80010001; goto cleanup; }
            // Inicializar textura a negro para evitar crash con datos vacíos
            uint8_t* datap = (uint8_t*)vita2d_texture_get_datap(frame_textures[i]);
            if (datap) memset(datap, 0, image_scaling.texture_width * image_scaling.texture_height * 4);
        }
        // Buffer intermedio de salida (decoder -> copia a textura)
        size_t out_needed = (size_t)image_scaling.texture_width * (size_t)image_scaling.texture_height * 4;
        decoder_out_rgba_size = out_needed;
        decoder_out_rgba = (uint8_t*)malloc(out_needed + 64); // margen + guardas
        if (!decoder_out_rgba) { VITA_DEBUG_LOG("[Video] Error: No hay memoria para decoder_out_rgba (%u bytes)", (unsigned)out_needed); ret = 0x80010001; goto cleanup; }
        memset(decoder_out_rgba, 0x7E, out_needed + 64);
        decoder_out_guard_pre = 0xCAFEBABE;
        decoder_out_guard_post = 0xDEADC0DE;
        frame_front_idx = 0; frame_back_idx = 1;
        // Usar doble buffer (single_frame_buffer = false) para reducir artefactos y evitar bloqueo decoder->present
        single_frame_buffer = false;
        legacy_single_immediate_present = false;
        VITA_DEBUG_LOG("[Video][INIT] single_frame_buffer=FALSE (doble buffer activo)");
        VITA_DEBUG_LOG("[Video][INIT] legacy_single_immediate_present=FALSE (present diferido)");
        VITA_DEBUG_LOG("[Video] Doble buffer de texturas inicializado %dx%d", image_scaling.texture_width, image_scaling.texture_height);

        decoder_src_width = width;
        decoder_src_height = height;
        decoder_tried_direct_texture = false;
        // Si estamos en modo single frame buffer probamos directamente la ruta DIRECT_TEXTURE (baseline legacy)
        // Inicialmente intentar DIRECT_TEXTURE; si falla (invalid pointer) se hace fallback dinámico
        decoder_output_mode = DECODER_OUT_DIRECT_TEXTURE;
        VITA_DEBUG_LOG("[Video][INIT] decoder_output_mode inicial=DIRECT_TEXTURE (doble buffer)");
        // Reserva diferida de YUV: sólo si se activa DECODER_OUT_YUV_CONVERT en el futuro.
        if (decoder_output_mode == DECODER_OUT_YUV_CONVERT) {
            size_t yuv_needed = (size_t)width * (size_t)height * 3 / 2;
            decoder_yuv_buffer_size = yuv_needed;
            decoder_yuv_total_alloc = yuv_needed + 32;
            decoder_yuv_raw = (uint8_t*)malloc(decoder_yuv_total_alloc);
            if (!decoder_yuv_raw) { VITA_DEBUG_LOG("[Video] Error: No hay memoria para decoder_yuv_raw (%u bytes)", (unsigned)decoder_yuv_total_alloc); ret = 0x80010001; goto cleanup; }
            decoder_yuv_buffer = decoder_yuv_raw + 16;
            yuv_write_canaries();
            VITA_DEBUG_LOG("[Video] Buffer YUV asignado (%u + canarios)", (unsigned)yuv_needed);
        } else {
            decoder_yuv_raw = nullptr; decoder_yuv_buffer = nullptr; decoder_yuv_buffer_size = 0; decoder_yuv_total_alloc = 0;
            VITA_DEBUG_LOG("[Video] Ruta RGBA directa: sin reservar buffer YUV (diferido)");
        }
        VITA_DEBUG_LOG("[Video] Framebuffer inicializado");
        video_status = VITA_VIDEO_INIT_FRAMEBUFFER;
    }
    if (video_status == VITA_VIDEO_INIT_FRAMEBUFFER) {
    // Low latency removido: la ruta inmediata es siempre usada (comentario informativo)
    VITA_DEBUG_LOG("[Video][SETUP] Ruta inmediata activa (low latency eliminado)");
        video_fullscreen_stretch = g_video_settings_snapshot.fullscreen;
        // Configurar pacing target fps si se conoce
        if (g_stats.target_fps == 0 && redrawRate > 0) {
            g_stats.target_fps = (uint32_t)redrawRate;
        }
        vita_netopt_set_target_fps(g_stats.target_fps ? g_stats.target_fps : 60);
        if (!init) { init = (SceVideodecQueryInitInfoHwAvcdec*)calloc(1, sizeof(*init)); if (!init) { ret = 0x80010001; goto cleanup; } }
        init->size = sizeof(*init);
        init->horizontal = VITA_DECODER_RESOLUTION(width);
        init->vertical = VITA_DECODER_RESOLUTION(height);
        init->numOfRefFrames = 4; init->numOfStreams = 1;
        decoder_width = init->horizontal;
        decoder_height = init->vertical;
        // No need for RGBA buffer, decoding directly to texture
        // Inicializar SPS context como en legacy
        if (!g_sps_ctx) {
            g_sps_ctx = new gs::SpsContext(decoder_width, decoder_height);
        }
        ret = sceVideodecInitLibrary(SCE_VIDEODEC_TYPE_HW_AVCDEC, init);
        if (ret < 0) { VITA_DEBUG_LOG("[Video] Error sceVideodecInitLibrary: 0x%x", ret); ret = 0x80010002; goto cleanup; }
        video_status = VITA_VIDEO_INIT_AVC_LIB;
    }
    if (video_status == VITA_VIDEO_INIT_AVC_LIB) {
        if (!decoder_info) { decoder_info = (SceAvcdecQueryDecoderInfo*)calloc(1, sizeof(*decoder_info)); if (!decoder_info) { ret = 0x80010001; goto cleanup; } }
        decoder_info->horizontal = init->horizontal; decoder_info->vertical = init->vertical; decoder_info->numOfRefFrames = init->numOfRefFrames;
        SceAvcdecDecoderInfo decoder_info_out = {0};
        ret = sceAvcdecQueryDecoderMemSize(SCE_VIDEODEC_TYPE_HW_AVCDEC, decoder_info, &decoder_info_out);
        if (ret < 0) { VITA_DEBUG_LOG("[Video] Error sceAvcdecQueryDecoderMemSize: 0x%x", ret); ret = 0x80010003; goto cleanup; }
        decoder = (SceAvcdecCtrl*)calloc(1, sizeof(SceAvcdecCtrl));
        if (!decoder) { ret = 0x80010001; goto cleanup; }
        size_t sz = (decoder_info_out.frameMemSize + 0xFFFFF) & ~0xFFFFF;
        decoder->frameBuf.size = sz;
        decoderblock = sceKernelAllocMemBlock("decoder", SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW, sz, NULL);
        if (decoderblock < 0) { VITA_DEBUG_LOG("[Video] Error decoderblock: 0x%08x", decoderblock); ret = 0x80010004; goto cleanup; }
        ret = sceKernelGetMemBlockBase(decoderblock, &decoder->frameBuf.pBuf);
        if (ret < 0) { VITA_DEBUG_LOG("[Video] Error sceKernelGetMemBlockBase: 0x%x", ret); ret = 0x80010005; goto cleanup; }
        video_status = VITA_VIDEO_INIT_DECODER_MEMBLOCK;
    }
    if (video_status == VITA_VIDEO_INIT_DECODER_MEMBLOCK) {
        VITA_DEBUG_LOG("[Video] Creando decoder AVC...");
        ret = sceAvcdecCreateDecoder(SCE_VIDEODEC_TYPE_HW_AVCDEC, decoder, decoder_info);
        if (ret < 0) { VITA_DEBUG_LOG("[Video] Error sceAvcdecCreateDecoder: 0x%x", ret); ret = 0x80010006; goto cleanup; }
        video_status = VITA_VIDEO_INIT_AVC_DEC;
    }
    if (video_status == VITA_VIDEO_INIT_AVC_DEC) {
        ret = sceKernelCreateThread("frame_pacer", vita_pacer_thread_main, 0, 0x10000, 0, 0, NULL);
        if (ret < 0) { VITA_DEBUG_LOG("[Video] Error creando thread frame_pacer: 0x%x", ret); ret = 0x80010007; goto cleanup; }
        pacer_thread = ret; active_pacer_thread = true; sceKernelStartThread(pacer_thread, 0, NULL);
        video_status = VITA_VIDEO_INIT_FRAME_PACER_THREAD;
    }
    // Configuración inicial: direct output ON, pure copy ON, sin síntesis de start codes (fallback los activará si es necesario)
    vitavideo_set_legacy_direct_output_mode(true);
    vitavideo_set_legacy_pure_copy_mode(true);
    legacy_synthesize_startcodes_in_pure = false;
    pure_copy_failure_count = 0;
    VITA_DEBUG_LOG("[Video][INIT] direct_output=1 pure_copy=1 synth_startcodes=0");
    VITA_DEBUG_LOG("[Video] vitavideo_setup completado exitosamente, retornando 0");
    if (video_status_canary_pre != 0xDEADBEEF || video_status_canary_post != 0xBAADF00D) {
        VITA_DEBUG_LOG("[Video][CORRUPT] Canarios video_status alterados despues de setup pre=0x%08X post=0x%08X", video_status_canary_pre, video_status_canary_post);
    }
    return 0;
cleanup:
    vita_cleanup();
    return ret;
}