#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string>
#include <Limelight.h>

// Forward declarations para evitar incluir libgamestream directamente
struct Data;
struct _APP_LIST;
typedef struct _APP_LIST APP_LIST, *PAPP_LIST;

// Estructuras para el sistema de video FFmpeg
typedef struct {
    void *codec_context;
    void *frame;
    void *packet;
    bool initialized;
} FFmpegVideoDecoder;

typedef struct {
    void *texture;
    int width;
    int height;
    bool has_frame;
} FFmpegVideoFrame;

typedef struct {
    FFmpegVideoDecoder decoder;
    FFmpegVideoFrame current_frame;
    int frame_rate;
    bool is_legacy_mode;
    std::string render_mode;
} FFmpegVideoContext;

// Funciones del sistema de video FFmpeg
extern "C" {

// Inicialización y cleanup
int ffmpeg_video_init(FFmpegVideoContext *context, int width, int height, int frame_rate);
void ffmpeg_video_cleanup(FFmpegVideoContext *context);

// Control del video
void ffmpeg_video_start(FFmpegVideoContext *context);
void ffmpeg_video_stop(FFmpegVideoContext *context);

// Decodificación de frames
int ffmpeg_video_decode(FFmpegVideoContext *context, unsigned char *data, int size, int frame_type);
void ffmpeg_video_render(FFmpegVideoContext *context);

// Callbacks para Limelight
DECODER_RENDERER_CALLBACKS get_ffmpeg_video_callbacks(void);

// Utilidades
void ffmpeg_video_set_render_mode(FFmpegVideoContext *context, const char *mode);
const char* ffmpeg_video_get_render_mode(FFmpegVideoContext *context);

}
