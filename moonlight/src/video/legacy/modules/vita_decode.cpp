#include "vita_globals.h"
#include <psp2/videodec.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <vita2d.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "libgamestream/sps.h"
#include <borealis/core/application.hpp>
#include <borealis/extern/nanovg/nanovg.h>
#include "debug.h"
#include "../../../network/NetworkOptimizations.h"

extern "C" uint64_t LiGetMillis();
static inline uint64_t monotonicMs_local() {
    if (LiGetMillis) return LiGetMillis();
    return 0; // si retorna 0 perderemos timing pero no rompe
}


// Función para procesar unidades de decodificación
extern "C" int vitavideo_submit_decode_unit(PDECODE_UNIT decodeUnit) {
    static uint32_t vd_submit_counter = 0;
    if (vd_submit_counter < 4 || (vd_submit_counter % 120) == 0) {
        VITA_DEBUG_LOG("[Video] submit_decode_unit #%u", vd_submit_counter);
    }
    SceAvcdecAu au = {0};
    SceAvcdecArrayPicture array_picture = {0};
    struct SceAvcdecPicture picture = {0};
    struct SceAvcdecPicture *pictures = { &picture };
    array_picture.numOfElm = 1;
    array_picture.pPicture = &pictures;

    picture.size = sizeof(picture);
    picture.frame.pixelType = 0; // SCE_AVCDEC_PIXELFORMAT_RGBA8888
    picture.frame.framePitch = image_scaling.texture_width;   // pitch en pixels (igual que legacy)
    picture.frame.frameWidth = image_scaling.texture_width;
    picture.frame.frameHeight = image_scaling.texture_height;

    // Importante: SceAvcdecFrame solo tiene pPicture[2]. NO escribir índice 2 (overflow)!
    // Esto estaba provocando corrupción de memoria que luego causaba crash en vita2d_draw_texture_part.

    uint8_t* tex = nullptr;
    if (decoder_output_mode == DECODER_OUT_DIRECT_TEXTURE) {
        // En modo single frame buffer no hay BACK separado: escribir directamente en FRONT
        vita2d_texture* target_tex = single_frame_buffer ? FRAME_FRONT() : FRAME_BACK();
        tex = (uint8_t*)vita2d_texture_get_datap(target_tex);
        if (!tex) {
            VITA_DEBUG_LOG("[Video][ERR] FRAME_BACK() datap NULL");
            return DR_NEED_IDR;
        }
        picture.frame.pPicture[0] = tex;
    } else if (decoder_output_mode == DECODER_OUT_PHYS_BUFFER_COPY) {
        // Asegurar buffer físico
        size_t needed = (size_t)image_scaling.texture_width * (size_t)image_scaling.texture_height * 4;
        if (!decoder_output_phys_ptr || decoder_output_phys_size < needed) {
            if (decoder_output_phys_block >= 0) {
                sceKernelFreeMemBlock(decoder_output_phys_block);
                decoder_output_phys_block = -1; decoder_output_phys_ptr = nullptr; decoder_output_phys_size = 0;
            }
            SceKernelAllocMemBlockOpt opt = {0};
            decoder_output_phys_block = sceKernelAllocMemBlock("dec_out_phys", SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW, (int)needed, &opt);
            if (decoder_output_phys_block < 0) {
                VITA_DEBUG_LOG("[Video][ERR] fallo alloc phys out 0x%08X", decoder_output_phys_block);
                return DR_NEED_IDR;
            }
            if (sceKernelGetMemBlockBase(decoder_output_phys_block, &decoder_output_phys_ptr) < 0) {
                VITA_DEBUG_LOG("[Video][ERR] fallo get base phys out");
                return DR_NEED_IDR;
            }
            decoder_output_phys_size = needed;
            VITA_DEBUG_LOG("[Video][FB] phys out alloc %u bytes ptr=%p", (unsigned)needed, decoder_output_phys_ptr);
        }
        tex = (uint8_t*)decoder_output_phys_ptr;
        picture.frame.pPicture[0] = tex;
    }
    picture.frame.pPicture[1] = NULL; // Solo dos entradas válidas (índices 0 y 1)

    // Sentinela para detectar overwrite si el decoder escribiera fuera
    uint32_t tail_sentinel = 0xA55A3CC3;
    // Colocar el sentinela justo tras la estructura picture (en stack) copiándolo a una variable volátil después
    volatile uint32_t* sentinel_ptr = &tail_sentinel;
    (void)sentinel_ptr; // evitar warning

    // Ensure buffer size
    if (decoder_buffer_size < (decodeUnit->fullLength + AV_INPUT_BUFFER_PADDING_SIZE)) {
        decoder_buffer = (char*)realloc(decoder_buffer, decodeUnit->fullLength + AV_INPUT_BUFFER_PADDING_SIZE);
        decoder_buffer_size = decodeUnit->fullLength + AV_INPUT_BUFFER_PADDING_SIZE;
        if (decoder_buffer == NULL) {
            return DR_NEED_IDR;
        }
    }

    // Simple copy like legacy
    PLENTRY entry = decodeUnit->bufferList;
    uint32_t length = 0;
    while (entry != NULL) {
        if (entry->bufferType == BUFFER_TYPE_SPS) {
            // Aplicar SPS fix como en legacy
            if (g_sps_ctx) {
                uint32_t old_length = length;
                g_sps_ctx->fix(entry, GS_SPS_BITSTREAM_FIXUP, (uint8_t*)decoder_buffer, &length);
                VITA_DEBUG_LOG("[Video] SPS fix: old_length=%u new_length=%u", old_length, length);
            } else {
                memcpy(decoder_buffer + length, entry->data, entry->length);
                length += entry->length;
            }
        } else {
            memcpy(decoder_buffer + length, entry->data, entry->length);
            length += entry->length;
        }
        entry = entry->next;
    }

    au.es.pBuf = decoder_buffer;
    au.es.size = decodeUnit->fullLength; // Like legacy
    au.dts.lower = au.dts.upper = au.pts.lower = au.pts.upper = 0xFFFFFFFF;

    if (vd_submit_counter < 2) {
        VITA_DEBUG_LOG("[Video] AU size=%u fullLength=%u", au.es.size, decodeUnit->fullLength);
    }

    uint64_t tDecodeStart = monotonicMs_local();
    int ret = sceAvcdecDecode(decoder, &au, &array_picture);
    uint64_t tDecodeEnd = monotonicMs_local();
    // Marca frame 'visto' (aunque falle decode, ya se intentó procesar AU)
    static unsigned syntheticFrameIndex = 0; // Hasta tener índice real
    vita_netopt_on_frame_seen(syntheticFrameIndex);
    if (ret < 0 || vd_submit_counter < 4 || (vd_submit_counter % 240) == 0) {
        VITA_DEBUG_LOG("[Video] dec ret=0x%x out=%d pitch=%u", ret, array_picture.numOfOutput, picture.frame.framePitch);
    }
    if (ret < 0) {
        VITA_DEBUG_LOG("[Video] sceAvcdecDecode error: 0x%x (mode=%d)", ret, (int)decoder_output_mode);
        if (ret == 0x80620009) { // INVALID_POINTER -> cambiar estrategia
            if (decoder_output_mode == DECODER_OUT_DIRECT_TEXTURE) {
                decoder_output_mode = DECODER_OUT_PHYS_BUFFER_COPY;
                VITA_DEBUG_LOG("[Video][FB] cambiando a PHYS_BUFFER_COPY y solicitando IDR");
                return DR_NEED_IDR; // forzar IDR en siguiente
            } else if (decoder_output_mode == DECODER_OUT_PHYS_BUFFER_COPY) {
                // Próximo paso futuro: DECODER_OUT_YUV_CONVERT
                VITA_DEBUG_LOG("[Video][FB] considerar ruta YUV (no implementada aún)");
            }
        }
        return DR_NEED_IDR;
    }

    if (array_picture.numOfOutput != 1) {
        syntheticFrameIndex++;
        return DR_OK;
    }

    // Verificar sentinela (detección temprana de corrupción de stack por overflow en pPicture)
    if (tail_sentinel != 0xA55A3CC3) {
        VITA_DEBUG_LOG("[Video][CORRUPT] tail_sentinel modificado tras decode (0x%08X)", tail_sentinel);
    }

    // Inspeccionar primeros bytes del frame recién decodificado para diagnosticar si el decoder escribe
    if (array_picture.numOfOutput == 1) {
        // Si usamos buffer físico, copiar ahora a la textura BACK
        if (decoder_output_mode == DECODER_OUT_PHYS_BUFFER_COPY) {
            vita2d_texture* target_tex = single_frame_buffer ? FRAME_FRONT() : FRAME_BACK();
            uint8_t* back = (uint8_t*)vita2d_texture_get_datap(target_tex);
            size_t bytes = (size_t)image_scaling.texture_width * (size_t)image_scaling.texture_height * 4;
            if (back) memcpy(back, tex, bytes);
        }
        uint8_t sample[16];
        vita2d_texture* inspect_tex = single_frame_buffer ? FRAME_FRONT() : FRAME_BACK();
        uint8_t* inspect = (uint8_t*)vita2d_texture_get_datap(inspect_tex);
        static uint32_t vd_output_frame_counter = 0;
        if (inspect) {
            if (vd_output_frame_counter < 2 || (vd_output_frame_counter % 240) == 0) {
                memcpy(sample, inspect, 16);
                uint32_t accum = 0; for (int i=0;i<16;i++) accum = (accum * 131) + sample[i];
                VITA_DEBUG_LOG("[Video][DECODE][CHK] frame#%u csum=0x%08X b0=%02X b1=%02X b2=%02X b3=%02X", vd_output_frame_counter, accum, sample[0], sample[1], sample[2], sample[3]);
            }
        }
        // Siempre hacer staging copy para overlays (ya no dependemos de low_latency flag)
        if (inspect && decoder_out_rgba) {
            size_t frame_bytes = (size_t)image_scaling.texture_width * (size_t)image_scaling.texture_height * 4;
            if (decoder_out_rgba_size >= frame_bytes) {
                memcpy(decoder_out_rgba, inspect, frame_bytes);
                video_nvg_frame_dirty = true;
                static uint32_t nvg_dirty_counter = 0;
                if (nvg_dirty_counter < 2 || (nvg_dirty_counter % 300) == 0) {
                    VITA_DEBUG_LOG("[Video][NVG][STAGE] copy->staging #%u", nvg_dirty_counter);
                }
                nvg_dirty_counter++;
            }
        }
        vd_output_frame_counter++;
    }

    // Ya decodificado directamente a la textura

    // Swap buffers (si usamos doble buffer) antes de posible present inmediato
    if (!single_frame_buffer && !legacy_single_immediate_present) {
        int old_front = frame_front_idx;
        frame_front_idx = frame_back_idx;
        frame_back_idx = old_front;
    }

    bool did_immediate_present = false;
    uint64_t tPresentMs = 0;
    // Presentación inmediata ahora SIEMPRE (optimización unificada)
    vita2d_start_drawing();
    vita2d_clear_screen();
    {
        vita2d_texture* present_tex = FRAME_FRONT();
        if (present_tex) {
            int dw = video_fullscreen_stretch ? SCREEN_WIDTH : image_scaling.display_width;
            int dh = video_fullscreen_stretch ? SCREEN_HEIGHT : image_scaling.display_height;
            int ox = video_fullscreen_stretch ? 0 : image_scaling.offset_x;
            int oy = video_fullscreen_stretch ? 0 : image_scaling.offset_y;
            vita2d_draw_texture_tint_part_scale(present_tex, (float)ox, (float)oy,
                                                image_scaling.region_x1, image_scaling.region_y1,
                                                image_scaling.region_x2, image_scaling.region_y2,
                                                (float)dw / image_scaling.texture_width,
                                                (float)dh / image_scaling.texture_height,
                                                0xFFFFFFFF);
        }
    }
    vita2d_end_drawing();
    vita2d_swap_buffers();
    tPresentMs = monotonicMs_local();
    did_immediate_present = true;

    if (active_video_thread) {
        // Señalar frame producido para pacing adaptativo
        vita_netopt_frame_produced();
        // Gestión de frames drop/present
        if (need_drop > 0) {
            need_drop--;
            g_stats.frames_dropped_pacer++;
        } else {
            // Siempre contabilizamos inmediatamente (modo unificado)
            g_stats.frames_decoded++;
            frame_count++;
            vita_netopt_on_frame_completed(syntheticFrameIndex);
        }
    }
    // Instrumentación timing (solo si decode OK y hubo salida)
    if (array_picture.numOfOutput == 1) {
        vita_netopt_on_frame_timing(tDecodeStart, tDecodeEnd, tPresentMs);
    }
    syntheticFrameIndex++;

    vd_submit_counter++;
    return DR_OK;
}