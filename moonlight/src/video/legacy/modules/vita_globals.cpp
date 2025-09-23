#include "vita_globals.h"
#include "libgamestream/sps.h"

// Variables globales del sistema de video
VideoStatus video_status = VITA_VIDEO_NOT_INIT;
uint32_t video_status_canary_pre = 0xDEADBEEF;
uint32_t video_status_canary_post = 0xBAADF00D;

// Buffers y recursos decoder
char* decoder_buffer = NULL;
size_t decoder_buffer_size = 0;
SceAvcdecCtrl* decoder = NULL;
int decoderblock = -1;
SceAvcdecQueryDecoderInfo* decoder_info = NULL;
SceVideodecQueryInitInfoHwAvcdec* init = NULL;

// Doble buffer de texturas RGBA para salida directa
vita2d_texture* frame_textures[2] = { nullptr, nullptr };
int frame_front_idx = 0;
int frame_back_idx = 1;
bool single_frame_buffer = false; // por defecto doble buffer; se puede activar para pruebas legacy

// Escalado
image_scaling_settings image_scaling = {0};

// Estado de hilos y flags runtime
bool active_video_thread = false;
bool external_present_enabled = true;
bool standalone_present_mode = true;
bool frame_ready_flag = false;
uint32_t frame_count = 0;
int need_drop = 0;

// Modo legacy y heurísticas
bool legacy_direct_output_mode = true;
bool legacy_strict_parity_mode = false;
bool legacy_pure_copy_mode = true;
bool legacy_synthesize_startcodes_in_pure = false;
int pure_copy_failure_count = 0;
bool legacy_strict_diagnostic_mode = true;

// Heurística HEVC abort
bool hevc_abort_flag = false;

// Buffers YUV para ruta no-direct-output
uint8_t* decoder_yuv_raw = NULL;
uint8_t* decoder_yuv_buffer = NULL;
size_t decoder_yuv_buffer_size = 0;
size_t decoder_yuv_total_alloc = 0;
uint8_t* decoder_rgba_buffer = nullptr;
uint8_t* decoder_out_rgba = nullptr;
size_t decoder_out_rgba_size = 0;
uint32_t decoder_out_guard_pre = 0;
uint32_t decoder_out_guard_post = 0;
void* decoder_output_phys_ptr = nullptr;
size_t decoder_output_phys_size = 0;
int decoder_output_phys_block = -1;
int video_nvg_image_id = 0;
bool video_nvg_image_ready = false;
volatile bool video_nvg_frame_dirty = false;
bool legacy_single_immediate_present = false;
bool video_fullscreen_stretch = true; // activado por defecto para ocupar pantalla completa
// low-latency eliminado: presentación inmediata ahora es el camino único
DecoderOutputMode decoder_output_mode = DECODER_OUT_DIRECT_TEXTURE;
bool decoder_tried_direct_texture = false;
int decoder_src_width = 0;
int decoder_src_height = 0;
int decoder_width = 0;
int decoder_height = 0;

// Estadísticas
VitaVideoStats g_stats = {0};
uint64_t stats_start_ms = 0;
uint64_t last_fps_window_ms = 0;
uint32_t curr_fps[2] = {0,0};
SceUID pacer_thread = -1;
bool active_pacer_thread = false;

// Indicadores
indicator_status poor_net_indicator = {0};

// Contexto SPS
gs::SpsContext* g_sps_ctx = nullptr;

// Video status info
VideoStatusInfo g_video_status_info = {VITA_VIDEO_NOT_INIT};

// Video settings snapshot
// Defined elsewhere

// Funciones
void vitavideo_update_scaling_settings(int width, int height) {
    float screen_aspect = (float)SCREEN_WIDTH / SCREEN_HEIGHT;
    float video_aspect = (float)width / height;
    if (video_aspect > screen_aspect) {
        image_scaling.texture_width = SCREEN_WIDTH;
        image_scaling.texture_height = (int)(SCREEN_WIDTH / video_aspect);
        image_scaling.origin_x = 0;
        image_scaling.origin_y = (SCREEN_HEIGHT - image_scaling.texture_height) / 2;
    } else {
        image_scaling.texture_width = (int)(SCREEN_HEIGHT * video_aspect);
        image_scaling.texture_height = SCREEN_HEIGHT;
        image_scaling.origin_x = (SCREEN_WIDTH - image_scaling.texture_width) / 2;
        image_scaling.origin_y = 0;
    }
    image_scaling.region_x1 = 0.0f;
    image_scaling.region_y1 = 0.0f;
    image_scaling.region_x2 = (float)width;
    image_scaling.region_y2 = (float)height;
    image_scaling.enabled = true;
    image_scaling.display_width = image_scaling.texture_width;
    image_scaling.display_height = image_scaling.texture_height;
    image_scaling.offset_x = image_scaling.origin_x;
    image_scaling.offset_y = image_scaling.origin_y;
}

void yuv_write_canaries() {
    if (!decoder_yuv_raw || decoder_yuv_total_alloc < 32) return;
    decoder_yuv_raw[0]=0xCA; decoder_yuv_raw[1]=0xFE;
    decoder_yuv_raw[decoder_yuv_total_alloc-1]=0xBE;
    decoder_yuv_raw[decoder_yuv_total_alloc-2]=0xEF;
}

bool yuv_check_canaries() {
    if (!decoder_yuv_raw || decoder_yuv_total_alloc < 32) return true;
    return decoder_yuv_raw[0]==0xCA && decoder_yuv_raw[1]==0xFE &&
           decoder_yuv_raw[decoder_yuv_total_alloc-1]==0xBE && decoder_yuv_raw[decoder_yuv_total_alloc-2]==0xEF;
}