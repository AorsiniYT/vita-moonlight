#include "ffmpeg.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Limelight.h>

#include "libgamestream/client.h"
#include "libgamestream/errors.h"

#include <borealis/core/logger.hpp>

#include <mutex>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

#include "legacy/modules/vita_globals.hpp"
#include "video/VideoFrameHolder.hpp"
#include "session/vita_session.hpp"
#include "network/NetworkOptimizations.hpp"

#ifdef BOREALIS_USE_GXM
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/videodec.h>
#include <vita2d.h>
#endif

static FFmpegVideoContext* g_ffmpeg_context = nullptr;
static std::mutex g_ffmpeg_mutex;
static uint32_t g_ffmpeg_submit_counter = 0;
static unsigned g_ffmpeg_frame_index = 0;

#ifdef BOREALIS_USE_GXM
struct dr_format_spec {
    enum AVPixelFormat ff_format;
    SceGxmTextureFormat sce_format;
    uint32_t alignment_pitch;
};

static const dr_format_spec g_dr_format_spec_list[] = {
    { AV_PIX_FMT_RGBA,      SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR, 16 },
    { AV_PIX_FMT_BGR565LE,  SCE_GXM_TEXTURE_FORMAT_U5U6U5_BGR,    16 },
    { AV_PIX_FMT_BGR555LE,  SCE_GXM_TEXTURE_FORMAT_U1U5U5U5_ABGR, 16 },
    { AV_PIX_FMT_YUV420P,   SCE_GXM_TEXTURE_FORMAT_YUV420P3_CSC0, 32 },
    { AV_PIX_FMT_NV12,      SCE_GXM_TEXTURE_FORMAT_YVU420P2_CSC0, 16 },
};

static const dr_format_spec* get_dr_format_spec(enum AVPixelFormat fmt) {
    for (unsigned i = 0; i < sizeof(g_dr_format_spec_list) / sizeof(g_dr_format_spec_list[0]); ++i) {
        if (g_dr_format_spec_list[i].ff_format == fmt) {
            return &g_dr_format_spec_list[i];
        }
    }
    return nullptr;
}

static std::mutex g_dr_mutex;

static void vram_free(void* opaque, uint8_t* data) {
    SceUID mb = (SceUID)(intptr_t)opaque;
    if (data) {
        sceGxmUnmapMemory(data);
    }
    sceKernelFreeMemBlock(mb);
}

static bool vram_alloc(int* size, SceUID* mb, void** ptr) {
    *size = FFALIGN(*size, 256 * 1024);
    SceUID block = sceKernelAllocMemBlock("ffmpeg_gpu_mem",
                                          SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
                                          *size,
                                          nullptr);
    if (block < 0) {
        return false;
    }

    void* base = nullptr;
    if (sceKernelGetMemBlockBase(block, &base) != 0) {
        sceKernelFreeMemBlock(block);
        return false;
    }

    *mb = block;
    *ptr = base;
    return true;
}

extern "C" int get_buffer2_direct(AVCodecContext* avctx, AVFrame* pic, int /*flags*/) {
    const dr_format_spec* spec = get_dr_format_spec((enum AVPixelFormat)pic->format);
    if (!spec) {
        return AVERROR(EINVAL);
    }

    int width = FFMAX(FFALIGN(pic->width, 16), 64);
    int height = FFMAX(FFALIGN(pic->height, 16), 64);
    int pitch = FFALIGN(width, (int)spec->alignment_pitch);

    SceUID mb = 0;
    void* vram = nullptr;
    int size = av_image_get_buffer_size((enum AVPixelFormat)pic->format, pitch, height, 1);
    if (!vram_alloc(&size, &mb, &vram)) {
        return AVERROR(ENOMEM);
    }

    pic->buf[0] = av_buffer_create((uint8_t*)vram, size, vram_free, (void*)(intptr_t)mb, 0);
    if (!pic->buf[0]) {
        sceKernelFreeMemBlock(mb);
        return AVERROR(ENOMEM);
    }

    av_image_fill_arrays(pic->data,
                         pic->linesize,
                         (const uint8_t*)vram,
                         (enum AVPixelFormat)pic->format,
                         pitch,
                         height,
                         1);
    return 0;
}

struct dr_texture {
    vita2d_texture impl;
    AVFrame frame;
};

static dr_texture* dr_texture_alloc() {
    dr_texture* tex = (dr_texture*)malloc(sizeof(dr_texture));
    if (!tex) {
        return nullptr;
    }
    memset(tex, 0, sizeof(*tex));
    av_frame_unref(&tex->frame);
    return tex;
}

static void dr_texture_detach(dr_texture* tex) {
    if (!tex) {
        return;
    }
    AVBufferRef* buf = tex->frame.buf[0];
    if (!buf) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_dr_mutex);
    sceGxmUnmapMemory(buf->data);
    av_frame_unref(&tex->frame);
}

static void dr_texture_free(dr_texture** p_tex) {
    if (!p_tex || !*p_tex) {
        return;
    }
    dr_texture_detach(*p_tex);
    free(*p_tex);
    *p_tex = nullptr;
}

static void dr_texture_attach(dr_texture* tex, AVFrame* frame) {
    if (!tex || !frame) {
        return;
    }

    const dr_format_spec* spec = get_dr_format_spec((enum AVPixelFormat)frame->format);
    if (!spec) {
        return;
    }

    AVBufferRef* buf = frame->buf[0];
    if (!buf) {
        return;
    }

    int width = FFMAX(FFALIGN(frame->width, 16), 64);
    int height = FFMAX(FFALIGN(frame->height, 16), 64);

    std::lock_guard<std::mutex> lock(g_dr_mutex);
    sceGxmMapMemory(buf->data, buf->size, SCE_GXM_MEMORY_ATTRIB_READ);
    sceGxmTextureInitLinear(&tex->impl.gxm_tex, buf->data, spec->sce_format, width, height, 0);
    av_frame_unref(&tex->frame);
    av_frame_move_ref(&tex->frame, frame);
}
#endif // BOREALIS_USE_GXM

static void reset_global_slots() {
    frame_textures[0] = nullptr;
    frame_textures[1] = nullptr;
    frame_front_idx = 0;
    frame_back_idx = 0;
    single_frame_buffer = true;
}

static void free_decode_unit(PDECODE_UNIT unit) {
    if (!unit) {
        return;
    }
    PLENTRY entry = unit->bufferList;
    while (entry) {
        PLENTRY next = entry->next;
        if (entry->data) {
            free(entry->data);
        }
        free(entry);
        entry = next;
    }
    free(unit);
}

static uint64_t monotonic_ms_local() {
    return vita_monotonic_ms();
}

static int ensure_sw_texture(FFmpegVideoContext* ctx, int width, int height) {
#ifndef BOREALIS_USE_GXM
    (void)ctx;
    (void)width;
    (void)height;
    return -1;
#else
    if (width <= 0 || height <= 0) {
        return -1;
    }

    if (ctx->sw_texture && ctx->sw_texture_width == width && ctx->sw_texture_height == height) {
        return 0;
    }

    if (ctx->sw_texture) {
        vita2d_free_texture(ctx->sw_texture);
        ctx->sw_texture = nullptr;
    }

    ctx->sw_texture = vita2d_create_empty_texture_format(width, height, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR);
    if (!ctx->sw_texture) {
        return -1;
    }

    ctx->sw_texture_width = width;
    ctx->sw_texture_height = height;
    ctx->sw_texture_stride = vita2d_texture_get_stride(ctx->sw_texture);

    void* dst = vita2d_texture_get_datap(ctx->sw_texture);
    if (dst) {
        memset(dst, 0, (size_t)ctx->sw_texture_stride * (size_t)height);
    }
    return 0;
#endif
}

#ifdef BOREALIS_USE_GXM
static bool publish_direct_frame(FFmpegVideoContext* ctx, AVFrame* frame) {
    if (!frame || !frame->buf[0]) {
        return false;
    }

    if (!get_dr_format_spec((enum AVPixelFormat)frame->format)) {
        return false;
    }

    dr_texture* tex = reinterpret_cast<dr_texture*>(ctx->dr_texture);
    if (!tex) {
        tex = dr_texture_alloc();
        if (!tex) {
            return false;
        }
        ctx->dr_texture = tex;
    }

    dr_texture_detach(tex);
    dr_texture_attach(tex, frame);

    ctx->current_frame.texture = &tex->impl;
    ctx->current_frame.width = frame->width;
    ctx->current_frame.height = frame->height;
    ctx->current_frame.has_frame = true;
    ctx->current_frame.direct_memory = true;
    ctx->using_direct_memory = true;
    return true;
}
#endif

static bool publish_sw_frame(FFmpegVideoContext* ctx, AVFrame* frame) {
#ifndef BOREALIS_USE_GXM
    (void)ctx;
    (void)frame;
    return false;
#else
    if (!frame) {
        return false;
    }

    if (ensure_sw_texture(ctx, frame->width, frame->height) < 0) {
        return false;
    }

    ctx->sws_context = sws_getCachedContext(ctx->sws_context,
                                            frame->width,
                                            frame->height,
                                            (enum AVPixelFormat)frame->format,
                                            frame->width,
                                            frame->height,
                                            AV_PIX_FMT_RGBA,
                                            SWS_BILINEAR,
                                            nullptr,
                                            nullptr,
                                            nullptr);
    if (!ctx->sws_context) {
        brls::Logger::error("[FFMPEG] Unable to create swscale context for format {}", frame->format);
        return false;
    }

    uint8_t* dst = (uint8_t*)vita2d_texture_get_datap(ctx->sw_texture);
    if (!dst) {
        return false;
    }

    const uint8_t* inData[4] = { frame->data[0], frame->data[1], frame->data[2], frame->data[3] };
    int inStride[4] = { frame->linesize[0], frame->linesize[1], frame->linesize[2], frame->linesize[3] };
    uint8_t* outData[4] = { dst, nullptr, nullptr, nullptr };
    int outStride[4] = { ctx->sw_texture_stride, 0, 0, 0 };

    if (sws_scale(ctx->sws_context, inData, inStride, 0, frame->height, outData, outStride) <= 0) {
        brls::Logger::error("[FFMPEG] sws_scale failed");
        return false;
    }

    ctx->current_frame.texture = ctx->sw_texture;
    ctx->current_frame.width = frame->width;
    ctx->current_frame.height = frame->height;
    ctx->current_frame.has_frame = true;
    ctx->current_frame.direct_memory = false;
    ctx->using_direct_memory = false;
    return true;
#endif
}

static bool publish_frame(FFmpegVideoContext* ctx, AVFrame* frame, uint64_t ptsUs) {
    if (!ctx || !frame) {
        return false;
    }

    bool published = false;
#ifdef BOREALIS_USE_GXM
    published = publish_direct_frame(ctx, frame);
#endif
    if (!published) {
        published = publish_sw_frame(ctx, frame);
    }

    if (!published || !ctx->current_frame.texture) {
        return false;
    }

    reset_global_slots();
    frame_textures[0] = ctx->current_frame.texture;
    frame_textures[1] = ctx->current_frame.texture;
    frame_front_idx = 0;
    frame_back_idx = 0;
    single_frame_buffer = true;

    uint32_t texW = ctx->current_frame.width > 0 ? (uint32_t)ctx->current_frame.width : image_scaling.texture_width;
    uint32_t texH = ctx->current_frame.height > 0 ? (uint32_t)ctx->current_frame.height : image_scaling.texture_height;
    uint64_t ptsMs = ptsUs ? (ptsUs / 1000ULL) : monotonic_ms_local();

#ifdef BOREALIS_USE_GXM
    if (ctx->using_direct_memory) {
        dr_texture* tex = reinterpret_cast<dr_texture*>(ctx->dr_texture);
        if (tex) {
            AVBufferRef* buf = tex->frame.buf[0];
            if (buf) {
                vita2d_wait_rendering_done();
                sceGxmMapMemory(buf->data, buf->size, SCE_GXM_MEMORY_ATTRIB_READ);
            }
        }
    }
#endif

    ctx->last_pts_us = ptsUs;
    VideoFrameHolder::instance().pushTexture(ctx->current_frame.texture, texW, texH, ptsMs);
    VitaSession::onFrameDecoded();
    return true;
}

static void ffmpeg_release_locked(FFmpegVideoContext* ctx) {
    if (!ctx) {
        return;
    }

#ifdef BOREALIS_USE_GXM
    if (ctx->dr_texture) {
        dr_texture* tex = reinterpret_cast<dr_texture*>(ctx->dr_texture);
        dr_texture_free(&tex);
        ctx->dr_texture = nullptr;
    }
#endif
    if (ctx->sw_texture) {
        vita2d_free_texture(ctx->sw_texture);
        ctx->sw_texture = nullptr;
    }
    if (ctx->sws_context) {
        sws_freeContext(ctx->sws_context);
        ctx->sws_context = nullptr;
    }
    if (ctx->decoder.initialized) {
        ffmpeg_decoder_destroy(&ctx->decoder);
    }
    memset(&ctx->decoder, 0, sizeof(ctx->decoder));
    memset(&ctx->current_frame, 0, sizeof(ctx->current_frame));
    ctx->sw_texture_width = 0;
    ctx->sw_texture_height = 0;
    ctx->sw_texture_stride = 0;
    ctx->using_direct_memory = false;
    ctx->initialized = false;
    ctx->last_pts_us = 0;
    reset_global_slots();
}

extern "C" {

int ffmpeg_video_init(FFmpegVideoContext* context, int width, int height, int frame_rate) {
    if (!context) {
        return -1;
    }

    memset(context, 0, sizeof(*context));
    context->frame_rate = frame_rate;
    context->stream_width = width;
    context->stream_height = height;
    context->render_mode = "ffmpeg";
    context->is_legacy_mode = false;
    context->using_direct_memory = false;
    reset_global_slots();
    return 0;
}

void ffmpeg_video_cleanup(FFmpegVideoContext* context) {
    std::lock_guard<std::mutex> lock(g_ffmpeg_mutex);
    if (g_ffmpeg_context == context) {
        g_ffmpeg_context = nullptr;
    }
    ffmpeg_release_locked(context);
}

int ffmpeg_video_decode(FFmpegVideoContext* context, unsigned char* data, int size, int frame_type) {
    (void)frame_type;
    if (!context || !context->decoder.initialized) {
        return AVERROR(EINVAL);
    }
    return ffmpeg_decoder_decode(&context->decoder, data, size);
}

void ffmpeg_video_start(FFmpegVideoContext* context) {
    if (!context) {
        return;
    }
    stats_start_ms = monotonic_ms_local();
    g_stats.target_fps = context->frame_rate > 0 ? (uint32_t)context->frame_rate : 60;
}

void ffmpeg_video_stop(FFmpegVideoContext* context) {
    (void)context;
}

void ffmpeg_video_render(FFmpegVideoContext* context) {
    (void)context;
}

static int ffmpeg_video_setup(int videoFormat, int width, int height, int redrawRate, void* ctxPtr, int drFlags) {
    (void)videoFormat;
    (void)drFlags;

    auto* context = static_cast<FFmpegVideoContext*>(ctxPtr);
    if (!context) {
        brls::Logger::error("[FFMPEG] setup received null context");
        return -1;
    }

    std::lock_guard<std::mutex> lock(g_ffmpeg_mutex);
    brls::Logger::info("[FFMPEG] setup {}x{} @ {}fps ctx={:#x}", width, height, redrawRate, (uintptr_t)context);

    ffmpeg_release_locked(context);
    if (ffmpeg_video_init(context, width, height, redrawRate) < 0) {
        brls::Logger::error("[FFMPEG] init failed");
        return -1;
    }

#ifdef BOREALIS_USE_GXM
    if (!vita2d_inited) {
        if (width > 960 || height > 544) {
            vita2d_init_advanced(8 * 1024 * 1024);
        } else {
            vita2d_init();
        }
        vita2d_inited = true;
        vita2d_set_vblank_wait(0);
    }
#endif

    vitavideo_configure_screen_resolution(width);
    vitavideo_update_scaling_settings(width, height);
    video_fullscreen_stretch = g_video_settings_snapshot.fullscreen;

    memset(&g_stats, 0, sizeof(g_stats));
    stats_start_ms = 0;
    last_fps_window_ms = 0;
    frame_count = 0;
    need_drop = 0;
    g_stats.target_fps = redrawRate > 0 ? (uint32_t)redrawRate : 60;
    vita_netopt_set_target_fps(g_stats.target_fps);
    g_ffmpeg_submit_counter = 0;
    g_ffmpeg_frame_index = 0;

    if (ffmpeg_decoder_init(&context->decoder) < 0) {
        brls::Logger::error("[FFMPEG] decoder init failed");
        ffmpeg_release_locked(context);
        return -1;
    }

    context->initialized = true;
    g_ffmpeg_context = context;
    reset_global_slots();
    return 0;
}

static void ffmpeg_video_start_callback(void) {
    std::lock_guard<std::mutex> lock(g_ffmpeg_mutex);
    if (g_ffmpeg_context) {
        ffmpeg_video_start(g_ffmpeg_context);
    }
}

static void ffmpeg_video_stop_callback(void) {
    std::lock_guard<std::mutex> lock(g_ffmpeg_mutex);
    if (g_ffmpeg_context) {
        ffmpeg_video_stop(g_ffmpeg_context);
    }
}

static void ffmpeg_video_cleanup_callback(void) {
    std::lock_guard<std::mutex> lock(g_ffmpeg_mutex);
    if (g_ffmpeg_context) {
        ffmpeg_release_locked(g_ffmpeg_context);
        g_ffmpeg_context = nullptr;
    }
}

static int ffmpeg_video_submit_decode_unit(PDECODE_UNIT decodeUnit) {
    if (!decodeUnit) {
        return DR_NEED_IDR;
    }

    std::unique_lock<std::mutex> lock(g_ffmpeg_mutex);
    FFmpegVideoContext* context = g_ffmpeg_context;
    if (!context || !context->initialized || !context->decoder.initialized) {
        lock.unlock();
        free_decode_unit(decodeUnit);
        return DR_NEED_IDR;
    }

    AVCodecContext* avctx = context->decoder.avctx;
    AVPacket* pkt = context->decoder.pkt;
    AVFrame* frame = context->decoder.frame;

    while (true) {
        int drain = avcodec_receive_frame(avctx, frame);
        if (drain == AVERROR(EAGAIN) || drain == AVERROR_EOF) {
            break;
        }
        if (drain < 0) {
            brls::Logger::error("[FFMPEG] receive_frame drain error: 0x{:X}", drain);
            break;
        }
        if (publish_frame(context, frame, context->last_pts_us)) {
            av_frame_unref(frame);
        }
    }

    if (av_new_packet(pkt, decodeUnit->fullLength) < 0) {
        brls::Logger::error("[FFMPEG] av_new_packet failed size={}", decodeUnit->fullLength);
        lock.unlock();
        free_decode_unit(decodeUnit);
        return DR_NEED_IDR;
    }

    uint8_t* dst = pkt->data;
    PLENTRY entry = decodeUnit->bufferList;
    while (entry) {
        memcpy(dst, entry->data, entry->length);
        dst += entry->length;
        entry = entry->next;
    }

    pkt->pts = decodeUnit->presentationTimeUs;
    pkt->dts = decodeUnit->presentationTimeUs;
    if (decodeUnit->frameType == FRAME_TYPE_IDR) {
        pkt->flags |= AV_PKT_FLAG_KEY;
    } else {
        pkt->flags &= ~AV_PKT_FLAG_KEY;
    }

    int sendRes = avcodec_send_packet(avctx, pkt);
    av_packet_unref(pkt);
    if (sendRes < 0 && sendRes != AVERROR(EAGAIN)) {
        brls::Logger::error("[FFMPEG] send_packet error=0x{:X}", sendRes);
        lock.unlock();
        free_decode_unit(decodeUnit);
        return DR_NEED_IDR;
    }

    vita_netopt_on_frame_seen(g_ffmpeg_frame_index);

    bool producedFrame = false;
    while (true) {
        int ret = avcodec_receive_frame(avctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            brls::Logger::error("[FFMPEG] receive_frame error=0x{:X}", ret);
            lock.unlock();
            free_decode_unit(decodeUnit);
            return DR_NEED_IDR;
        }

        if (publish_frame(context, frame, decodeUnit->presentationTimeUs)) {
            producedFrame = true;
            if (stats_start_ms == 0) {
                stats_start_ms = monotonic_ms_local();
            }
            g_stats.frames_decoded++;
            frame_count++;
            vita_netopt_frame_produced();
            vita_netopt_on_frame_completed(g_ffmpeg_frame_index);
            g_ffmpeg_frame_index++;
        }
        av_frame_unref(frame);
    }

    g_ffmpeg_submit_counter++;
    lock.unlock();
    free_decode_unit(decodeUnit);
    return producedFrame ? DR_OK : DR_OK;
}

DECODER_RENDERER_CALLBACKS get_ffmpeg_video_callbacks(void) {
    DECODER_RENDERER_CALLBACKS callbacks = {0};
    callbacks.setup = ffmpeg_video_setup;
    callbacks.start = ffmpeg_video_start_callback;
    callbacks.stop = ffmpeg_video_stop_callback;
    callbacks.cleanup = ffmpeg_video_cleanup_callback;
    callbacks.submitDecodeUnit = ffmpeg_video_submit_decode_unit;
    callbacks.capabilities = CAPABILITY_DIRECT_SUBMIT | CAPABILITY_SLICES_PER_FRAME(2);
    return callbacks;
}

void ffmpeg_video_set_render_mode(FFmpegVideoContext* context, const char* mode) {
    if (!context) {
        return;
    }
    context->render_mode = mode;
    context->is_legacy_mode = (mode && strcmp(mode, "legacy") == 0);
}

const char* ffmpeg_video_get_render_mode(FFmpegVideoContext* context) {
    return context ? context->render_mode : "ffmpeg";
}

} // extern "C"
