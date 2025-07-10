#pragma once

#include <borealis/core/logger.hpp>
#include "client.h"
#include "errors.h"
#include "Limelight.h"

class StreamingManager {
public:
    StreamingManager();
    ~StreamingManager();

    // Inicia la sesión de streaming
    bool start(SERVER_DATA& server, STREAM_CONFIGURATION& streamConfig);

    // Detiene la sesión de streaming
    void stop();

private:
    // Funciones de callback estáticas para Limelight
    static void connection_stage_starting(int stage);
    static void connection_stage_complete(int stage);
    static void connection_stage_failed(int stage, int error_code);
    static void connection_started();
    static void connection_terminated(int error_code);
    static void log_message(const char* format, ...);

    static int video_decoder_setup(int video_format, int width, int height, int redraw_rate, void* context, int dr_flags);
    static void video_decoder_start();
    static void video_decoder_stop();
    static void video_decoder_cleanup();
    static int video_decoder_submit_decode_unit(PDECODE_UNIT decode_unit);

    static int audio_renderer_init(int audio_config, const POPUS_MULTISTREAM_CONFIGURATION opus_config, void* context, int ar_flags);
    static void audio_renderer_start();
    static void audio_renderer_stop();
    static void audio_renderer_cleanup();
    static void audio_renderer_decode_and_play_sample(char* sample_data, int sample_length);

    // Callbacks
    CONNECTION_LISTENER_CALLBACKS _conn_callbacks;
    DECODER_RENDERER_CALLBACKS _video_callbacks;
    AUDIO_RENDERER_CALLBACKS _audio_callbacks;

    bool _is_running = false;
};
