#include "vita_globals.h"
// Zero-copy eliminado: métricas mínimas

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
// Estado de inicialización de vita2d
bool vita2d_inited = false;

// Escalado
image_scaling_settings image_scaling = {0};

// Estado de hilos y flags runtime
bool active_video_thread = false;
bool frame_ready_flag = false;
uint32_t frame_count = 0;
int need_drop = 0;

// Modo legacy y heurísticas
// Modos legacy eliminados

// Heurística HEVC abort
bool hevc_abort_flag = false;

// Buffers YUV para ruta no-direct-output
uint8_t* decoder_yuv_raw = NULL;
uint8_t* decoder_yuv_buffer = NULL;
size_t decoder_yuv_buffer_size = 0;
size_t decoder_yuv_total_alloc = 0;
// Staging RGBA eliminado
void* decoder_output_phys_ptr = nullptr;
size_t decoder_output_phys_size = 0;
int decoder_output_phys_block = -1;
bool decoder_output_phys_mapped = false;
// Integración NVG eliminada (render directo)
bool legacy_single_immediate_present = false; // presentación diferida mediante Borealis
bool video_fullscreen_stretch = true; // activado por defecto para ocupar pantalla completa
// low-latency eliminado: presentación inmediata ahora es el camino único
int decoder_output_mode = 0; // 0=RGBA directo, 1=YUV420 experimental
bool decoder_tried_direct_texture = true;
int decoder_src_width = 0;
int decoder_src_height = 0;
int decoder_width = 0;
int decoder_height = 0;
bool decoder_use_phys_fallback = false;
uint8_t* decoder_linear_rgba = nullptr;
size_t decoder_linear_rgba_size = 0;
uint8_t* decoder_linear_rgba_guard = nullptr;
size_t decoder_linear_rgba_guard_size = 0;
size_t decoder_linear_rgba_total_alloc = 0;
uint32_t decoder_linear_rgba_pitch_pixels = 0;
uint32_t decoder_linear_rgba_height = 0;
int decoder_linear_rgba_memblock = -1;
bool decoder_linear_rgba_physically_backed = false;

// Estadísticas
VitaVideoStats g_stats = {0};
uint64_t stats_start_ms = 0;
uint64_t last_fps_window_ms = 0;
uint32_t curr_fps[2] = {0,0};
SceUID pacer_thread = -1;
bool active_pacer_thread = false;

// Indicadores
indicator_status poor_net_indicator = {0};

gs::SpsContext* g_sps_ctx = nullptr; // contexto SPS activo (fix de SPS)

// Video status info
VideoStatusInfo g_video_status_info = {VITA_VIDEO_NOT_INIT};

// Video settings snapshot
// Defined elsewhere

// Funciones
void vitavideo_update_scaling_settings(int width, int height) {
    // Mantener la textura con el tamaño ORIGINAL del stream para evitar overflow en decoder.
    image_scaling.texture_width = width;
    image_scaling.texture_height = height;

    // Calcular dimensiones de presentación (letterbox o fullscreen) independientemente.
    float screen_aspect = (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT;
    float video_aspect = (float)width / (float)height;
    int dispW, dispH, offX, offY;
    if (video_aspect > screen_aspect) {
        // Limita por ancho de pantalla
        dispW = SCREEN_WIDTH;
        dispH = (int)((float)SCREEN_WIDTH / video_aspect);
        offX = 0;
        offY = (SCREEN_HEIGHT - dispH) / 2;
    } else {
        // Limita por alto de pantalla
        dispH = SCREEN_HEIGHT;
        dispW = (int)((float)SCREEN_HEIGHT * video_aspect);
        offY = 0;
        offX = (SCREEN_WIDTH - dispW) / 2;
    }
    image_scaling.display_width = dispW;
    image_scaling.display_height = dispH;
    image_scaling.offset_x = offX;
    image_scaling.offset_y = offY;
    image_scaling.origin_x = offX; // mantener compatibilidad con código que use origin
    image_scaling.origin_y = offY;

    image_scaling.region_x1 = 0.0f;
    image_scaling.region_y1 = 0.0f;
    image_scaling.region_x2 = (float)width;  // igual al tamaño de la textura
    image_scaling.region_y2 = (float)height;
    image_scaling.enabled = true;
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