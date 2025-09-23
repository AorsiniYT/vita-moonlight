#include "vita_globals.h"
#include <psp2/kernel/threadmgr.h>
#include <psp2/display.h>
#include <psp2/videodec.h>
#include <vita2d.h>
#include <stdlib.h>
#include <string.h>

// Funciones de control y configuración
extern "C" void vitavideo_show_poor_net_indicator() {
    poor_net_indicator.activated = true;
}

extern "C" void vitavideo_hide_poor_net_indicator() {
    memset(&poor_net_indicator, 0, sizeof(indicator_status));
}

extern "C" int vitavideo_initialized() {
    return video_status != VITA_VIDEO_NOT_INIT;
}

extern "C" void vitavideo_set_legacy_direct_output_mode(bool enable) {
    legacy_direct_output_mode = enable;
    VITA_DEBUG_LOG("[Video][CFG] legacy_direct_output_mode=%d", (int)legacy_direct_output_mode);
}

extern "C" void vitavideo_set_legacy_strict_parity_mode(bool enable) {
    legacy_strict_parity_mode = enable;
    if (enable) {
        legacy_direct_output_mode = true; // implicar directo
        VITA_DEBUG_LOG("[Video][CFG] legacy_strict_parity_mode=1 (forzando direct_output y primer intento fullLength)");
    } else {
        VITA_DEBUG_LOG("[Video][CFG] legacy_strict_parity_mode=0");
    }
}

extern "C" void vitavideo_set_legacy_pure_copy_mode(bool enable) {
    legacy_pure_copy_mode = enable;
    if (enable) {
        VITA_DEBUG_LOG("[Video][CFG] legacy_pure_copy_mode=1 (start code synth off, heuristica HEVC off, au.es.size=fullLength)");
    } else {
        VITA_DEBUG_LOG("[Video][CFG] legacy_pure_copy_mode=0");
    }
}

extern "C" void vitavideo_set_legacy_strict_diagnostic_mode(bool enable) {
    legacy_strict_diagnostic_mode = enable;
    VITA_DEBUG_LOG("[Video][CFG] legacy_strict_diagnostic_mode=%d", (int)enable);
}

extern "C" void vitavideo_enable_external_present(bool enable) {
    external_present_enabled = enable;
    frame_ready_flag = false;
}

extern "C" void vitavideo_set_standalone_present_mode(bool enable) {
    standalone_present_mode = enable;
    VITA_DEBUG_LOG("[Video][CFG] standalone_present_mode=%d", (int)standalone_present_mode);
}

// Funciones de estadísticas
extern "C" void vitavideo_get_stats(VitaVideoStats* outStats) {
    if (!outStats) return;
    *outStats = g_stats;
    if (stats_start_ms) {
        outStats->session_ms = vita_monotonic_ms() - stats_start_ms;
    }
}

extern "C" void vitavideo_reset_stats() {
    memset(&g_stats, 0, sizeof(g_stats));
    stats_start_ms = vita_monotonic_ms();
    last_fps_window_ms = stats_start_ms;
}

// Exponer textura actual
extern "C" vita2d_texture* vitavideo_get_current_texture() {
    return FRAME_FRONT();
}