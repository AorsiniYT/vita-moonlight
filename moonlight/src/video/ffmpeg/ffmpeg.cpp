#include "ffmpeg.hpp"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
/* Use the project's Limelight public header via the include dirs configured by
    the top-level CMakeLists (third_party/moonlight-common-c/src is added). */
#include <Limelight.h>

#include "libgamestream/client.h"
#include "libgamestream/errors.h"

#ifdef BOREALIS_USE_GXM
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>
}
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/display.h>
#include <psp2/videodec.h>
#include <psp2/gxm.h>
#include <vita2d.h>
#endif

// Implementación inicial del wrapper FFmpeg para Vita

// Inicialización del contexto FFmpeg
int ffmpeg_video_init(FFmpegVideoContext *context, int width, int height, int frame_rate) {
    memset(context, 0, sizeof(FFmpegVideoContext));

    context->frame_rate = frame_rate;
    context->is_legacy_mode = false;
    context->render_mode = "ffmpeg";

    // Por ahora solo inicialización mínima; la integración completa con libavcodec
    // vendrá cuando se implemente el decoder. Aquí preparamos estructuras y
    // dejamos listo el lugar para get_buffer2_direct si se usa h264_vita.
    printf("FFmpeg video init: %dx%d @ %d fps\n", width, height, frame_rate);

    context->decoder.initialized = false; // Se inicializará en el futuro
    return 0;
}

void ffmpeg_video_cleanup(FFmpegVideoContext *context) {
    // TODO: Cleanup FFmpeg resources cuando se agregue el decoder
    printf("FFmpeg video cleanup\n");
}

int ffmpeg_video_decode(FFmpegVideoContext *context, unsigned char *data, int size, int frame_type) {
    // TODO: Implementar decodificación FFmpeg real
    printf("FFmpeg decode: %d bytes, frame_type: %d\n", size, frame_type);
    return -1; // Not implemented yet
}

// Control del video
void ffmpeg_video_start(FFmpegVideoContext *context) {
    printf("FFmpeg video started\n");
}

void ffmpeg_video_stop(FFmpegVideoContext *context) {
    printf("FFmpeg video stopped\n");
}

// Callbacks para Limelight (placeholders)
static int ffmpeg_video_setup(int videoFormat, int width, int height, int redrawRate, void *context, int drFlags) {
    FFmpegVideoContext *video_context = (FFmpegVideoContext *)context;
    return ffmpeg_video_init(video_context, width, height, redrawRate);
}

static void ffmpeg_video_start_callback(void) {
    printf("FFmpeg video start callback\n");
}

static void ffmpeg_video_stop_callback(void) {
    printf("FFmpeg video stop callback\n");
}

static void ffmpeg_video_cleanup_callback(void) {
    printf("FFmpeg video cleanup callback\n");
}

static int ffmpeg_video_submit_decode_unit(PDECODE_UNIT decodeUnit) {
    // TODO: Implementar submit decode unit usando libavcodec
    printf("FFmpeg submit decode unit - Not implemented yet\n");
    return -1; // Not implemented yet
}

DECODER_RENDERER_CALLBACKS get_ffmpeg_video_callbacks(void) {
    DECODER_RENDERER_CALLBACKS callbacks = {0};

    callbacks.setup = ffmpeg_video_setup;
    callbacks.start = ffmpeg_video_start_callback;
    callbacks.stop = ffmpeg_video_stop_callback;
    callbacks.cleanup = ffmpeg_video_cleanup_callback;
    callbacks.submitDecodeUnit = ffmpeg_video_submit_decode_unit;
    callbacks.capabilities = 0;

    return callbacks;
}

void ffmpeg_video_set_render_mode(FFmpegVideoContext *context, const char *mode) {
    context->render_mode = mode;
    context->is_legacy_mode = (strcmp(mode, "legacy") == 0);
}

const char* ffmpeg_video_get_render_mode(FFmpegVideoContext *context) {
    return context->render_mode;
}

#ifdef BOREALIS_USE_GXM
// --- Implementación DR allocator y utilidades basada en README-vita.md ---
struct dr_format_spec {
    enum AVPixelFormat ff_format;
    SceGxmTextureFormat sce_format;
    uint32_t alignment_pitch;
};

static const struct dr_format_spec dr_format_spec_list[] = {
    { AV_PIX_FMT_RGBA, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR, 16 },
    { AV_PIX_FMT_BGR565LE, SCE_GXM_TEXTURE_FORMAT_U5U6U5_BGR, 16 },
    { AV_PIX_FMT_BGR555LE, SCE_GXM_TEXTURE_FORMAT_U1U5U5U5_ABGR, 16 },
    { AV_PIX_FMT_YUV420P, SCE_GXM_TEXTURE_FORMAT_YUV420P3_CSC0, 32 },
    { AV_PIX_FMT_NV12, SCE_GXM_TEXTURE_FORMAT_YVU420P2_CSC0, 16 },
};
static const struct dr_format_spec *get_dr_format_spec(int fmt)
{
    for (int i = 0; i < (int)(sizeof(dr_format_spec_list)/sizeof(dr_format_spec_list[0])); i++) {
        if (dr_format_spec_list[i].ff_format == (enum AVPixelFormat)fmt)
            return &dr_format_spec_list[i];
    }
    return NULL;
}

static void vram_free(void *opaque, uint8_t *data)
{
    SceUID mb = (intptr_t) opaque;
    sceKernelFreeMemBlock(mb);
}

static bool vram_alloc(int *size, SceUID *mb, void **ptr)
{
    *size = FFALIGN(*size, 256 * 1024);
    SceUID m = sceKernelAllocMemBlock("gpu_mem",
                                      SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
                                      *size, NULL);
    if (m < 0)
        return false;

    void *p = NULL;
    if (sceKernelGetMemBlockBase(m, &p) != 0)
        return false;

    *mb = m;
    *ptr = p;
    return true;
}

extern "C" int get_buffer2_direct(AVCodecContext *avctx, AVFrame *pic, int flags)
{
    const struct dr_format_spec *spec = get_dr_format_spec(pic->format);
    if (!spec)
        return AVERROR_UNKNOWN;

    int width = FFMAX(FFALIGN(pic->width, 16), 64);
    int height = FFMAX(FFALIGN(pic->height, 16), 64);
    int pitch = FFALIGN(width, spec->alignment_pitch);

    SceUID mb = 0;
    void *vram = NULL;
    int size = av_image_get_buffer_size((enum AVPixelFormat)pic->format, pitch, height, 1);
    if (!vram_alloc(&size, &mb, &vram))
        return AVERROR_UNKNOWN;

    /* av_buffer_create expects uint8_t* data and a void* opaque. We store the memblock id
       encoded as a pointer-sized integer in the opaque parameter. */
    pic->buf[0] = av_buffer_create((uint8_t*)vram, size, vram_free, (void*)(intptr_t)mb, 0);
    av_image_fill_arrays(pic->data, pic->linesize, (const uint8_t*)vram, (enum AVPixelFormat)pic->format, pitch, height, 1);
    return 0;
}

// DR texture bundle
struct dr_texture {
    vita2d_texture impl;
    AVFrame frame;
};

static struct dr_texture *dr_texture_alloc()
{
    struct dr_texture *tex = (struct dr_texture*)malloc(sizeof(struct dr_texture));
    if (!tex) return NULL;
    memset(tex, 0, sizeof(struct dr_texture));
    av_frame_unref(&tex->frame);
    return tex;
}

static void dr_texture_detach(struct dr_texture *tex)
{
    AVBufferRef *buf = tex->frame.buf[0];
    if (!buf)
        return;

    sceGxmUnmapMemory((void*)buf->data);
    av_frame_unref(&tex->frame);
}

static void dr_texture_free(struct dr_texture **p_tex)
{
    if (!p_tex || !(*p_tex))
        return;

    dr_texture_detach(*p_tex);
    free(*p_tex);
    *p_tex = NULL;
}

static void dr_texture_attach(struct dr_texture *tex, AVFrame *frame)
{
    const struct dr_format_spec *spec = get_dr_format_spec(frame->format);
    if (!spec)
        return;

    AVBufferRef *buf = frame->buf[0];
    if (!buf)
        return;

    int width = FFMAX(FFALIGN(frame->width, 16), 64);
    int height = FFMAX(FFALIGN(frame->height, 16), 64);

    sceGxmMapMemory((void*)buf->data, buf->size, SCE_GXM_MEMORY_ATTRIB_READ);
    sceGxmTextureInitLinear(&tex->impl.gxm_tex, (void*)buf->data, spec->sce_format, width, height, 0);
    av_frame_unref(&tex->frame);
    av_frame_move_ref(&tex->frame, frame);
}

#endif // BOREALIS_USE_GXM
