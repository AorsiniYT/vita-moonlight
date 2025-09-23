#ifndef VITA_GLOBALS_H
#define VITA_GLOBALS_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <memory>

// VitaSDK headers
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/display.h>
#include <psp2/videodec.h>
#include <vita2d.h>

// Project headers
#include <Limelight.h>
#include "ConfigManager.hpp"
#include "debug.h"
#include "libgamestream/sps.h"

// --- Mitigación de colisiones de nombres con Borealis ---
// Limelight.h define macros para botones de ratón: BUTTON_LEFT / BUTTON_RIGHT / etc.
// Borealis (borealis/core/input.hpp) declara un enum ControllerButton con miembros
// BUTTON_LEFT, BUTTON_RIGHT, etc. Cuando las macros siguen definidas, el preprocesador
// reemplaza los identificadores por números dentro del enum provocando errores de sintaxis
// (ej: '0x03,' en vez de 'BUTTON_RIGHT,').
// Des-definimos solo los macros conflictivos aquí porque en este módulo (video) no se usan
// directamente los macros de ratón de Limelight; si se necesitan valores se pueden usar
// constantes explícitas (0x01 izquierda, 0x03 derecha) o acceder a Limelight.h directamente
// en otro TU que no incluya este header.
#ifdef BUTTON_LEFT
#undef BUTTON_LEFT
#endif
#ifdef BUTTON_RIGHT
#undef BUTTON_RIGHT
#endif
#ifdef BUTTON_MIDDLE
#undef BUTTON_MIDDLE
#endif
#ifdef BUTTON_X1
#undef BUTTON_X1
#endif
#ifdef BUTTON_X2
#undef BUTTON_X2
#endif

// Local headers
#include "../vita.h"

// Defines
#define VITA_DEBUG_LOG(...) vita_debug_log(__VA_ARGS__)

#define DECODER_BUFFER_SIZE (128 * 1024)
#define AV_INPUT_BUFFER_PADDING_SIZE 64

#define SCREEN_WIDTH 960
#define SCREEN_HEIGHT 544
#define LINE_SIZE 960
#define FRAMEBUFFER_SIZE (2 * 1024 * 1024)
#define FRAMEBUFFER_ALIGNMENT (256 * 1024)

// Pure copy failure threshold
#define PURE_COPY_FAILURE_THRESHOLD 3

// Video status info structure
typedef struct {
    VideoStatus status;
    uint32_t framesRendered;
    uint32_t framesDropped;
    uint32_t framesDecoded;
    uint32_t currentFps;
    uint32_t targetFps;
    uint64_t sessionMs;
    int decoderWidth;
    int decoderHeight;
    int displayWidth;
    int displayHeight;
    bool scalingEnabled;
} VideoStatusInfo;

// Simple VideoSettings structure
// Note: Using the existing VideoSettings from ConfigManager.hpp

// Extern declarations
extern VideoStatus video_status;
extern uint32_t video_status_canary_pre;
extern uint32_t video_status_canary_post;

extern char* decoder_buffer;
extern size_t decoder_buffer_size;
extern SceAvcdecCtrl* decoder;
extern int decoderblock;
extern SceAvcdecQueryDecoderInfo* decoder_info;
extern SceVideodecQueryInitInfoHwAvcdec* init;

extern vita2d_texture* frame_textures[2];
extern int frame_front_idx;
extern int frame_back_idx;
#define FRAME_FRONT() (frame_textures[frame_front_idx])
#define FRAME_BACK()  (frame_textures[frame_back_idx])
// Modo de un solo buffer (sin doble buffering). Cuando está activo, FRONT y BACK son el mismo índice.
extern bool single_frame_buffer;

extern image_scaling_settings image_scaling;

extern bool active_video_thread;
extern bool external_present_enabled;
extern bool standalone_present_mode;
extern bool frame_ready_flag;
extern uint32_t frame_count;
extern int need_drop;

extern bool legacy_direct_output_mode;
extern bool legacy_strict_parity_mode;
extern bool legacy_pure_copy_mode;
extern bool legacy_synthesize_startcodes_in_pure;
extern int pure_copy_failure_count;
extern bool legacy_strict_diagnostic_mode;

extern bool hevc_abort_flag;

extern uint8_t* decoder_yuv_raw;
extern uint8_t* decoder_yuv_buffer;
extern size_t decoder_yuv_buffer_size;
extern size_t decoder_yuv_total_alloc;
extern uint8_t* decoder_rgba_buffer;
extern uint8_t* decoder_out_rgba; // nuevo buffer intermedio de salida
extern size_t decoder_out_rgba_size;
extern uint32_t decoder_out_guard_pre;
extern uint32_t decoder_out_guard_post;
// Imagen NanoVG para integración Borealis (opción B)
extern int video_nvg_image_id;      // handle NanoVG
extern bool video_nvg_image_ready;  // true cuando hay datos actualizados
extern volatile bool video_nvg_frame_dirty; // indica que decoder_out_rgba tiene frame nuevo pendiente de subir a NVG

// Modo experimental: replicar comportamiento legacy (un solo buffer + decode->draw inmediato)
extern bool legacy_single_immediate_present; // si true, el submit decodifica y presenta inmediatamente (sin frame_ready_flag)
// Modo de presentación fullscreen (estira sin conservar aspect)
extern bool video_fullscreen_stretch;
// low-latency removido: la ruta inmediata es estándar

// Buffer físico para salida directa del decoder (cuando la textura no es aceptada)
extern void* decoder_output_phys_ptr;
extern size_t decoder_output_phys_size;
extern int decoder_output_phys_block; // SceUID

// Estrategia de salida del decoder
enum DecoderOutputMode {
    DECODER_OUT_DIRECT_TEXTURE = 0,   // intentar escribir a textura (legacy rápido)
    DECODER_OUT_PHYS_BUFFER_COPY = 1, // escribir a buffer físico y copiar a textura
    DECODER_OUT_YUV_CONVERT = 2       // decodificar YUV y convertir a RGBA
};
extern DecoderOutputMode decoder_output_mode;
extern bool decoder_tried_direct_texture;
extern int decoder_src_width;
extern int decoder_src_height;
extern int decoder_width;
extern int decoder_height;

extern VitaVideoStats g_stats;
extern uint64_t stats_start_ms;
extern uint64_t last_fps_window_ms;
extern uint32_t curr_fps[2];
extern SceUID pacer_thread;
extern bool active_pacer_thread;

extern indicator_status poor_net_indicator;

extern gs::SpsContext* g_sps_ctx;

extern VideoStatusInfo g_video_status_info;

extern ::VideoSettings g_video_settings_snapshot;

// Function declarations
void vitavideo_update_scaling_settings(int width, int height);
void yuv_write_canaries();
bool yuv_check_canaries();
uint64_t vita_monotonic_ms();
int vita_pacer_thread_main(SceSize args, void *argp);

#endif // VITA_GLOBALS_H