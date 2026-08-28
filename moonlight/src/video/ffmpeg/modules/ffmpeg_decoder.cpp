#include "ffmpeg/modules/ffmpeg_decoder.hpp"
#include "../ffmpeg.hpp"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include "legacy/modules/vita_globals.hpp"

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>
#ifdef __cplusplus
}
#endif

#ifdef BOREALIS_USE_GXM
 #ifdef __cplusplus
extern "C" {
 #endif
int get_buffer2_direct(AVCodecContext *avctx, AVFrame *pic, int flags);
 #ifdef __cplusplus
}
 #endif
#endif


int ffmpeg_decoder_init(FFmpegDecoderContext *ctx)
{
    if (!ctx) {
        return -1;
    }

    memset(ctx, 0, sizeof(*ctx));

    // Prefer vita-specific h264 decoder if available (it registers as h264_vita)
    const AVCodec *codec = avcodec_find_decoder_by_name("h264_vita");
    if (!codec) {
        // If the vita-specific decoder isn't found, list registered h264 decoders for diagnostics
        const AVCodec *it = NULL;
        VITA_DEBUG_LOG("[FFMPEG] h264_vita not found, examining registered decoders for AV_CODEC_ID_H264:");
        void *iter = NULL;
        while ((it = av_codec_iterate(&iter))) {
            if (it->id == AV_CODEC_ID_H264) {
                VITA_DEBUG_LOG("[FFMPEG] found h264 candidate: name=%s long=%s wrapper=%s caps=0x%X", it->name, it->long_name ? it->long_name : "", it->wrapper_name ? it->wrapper_name : "", it->capabilities);
            }
        }
        codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    }
    if (!codec) {
        fprintf(stderr, "FFmpeg decoder: could not find H264 decoder\n");
        return -1;
    }

    VITA_DEBUG_LOG("FFmpeg decoder: using codec %s (%s)", codec->name, codec->long_name ? codec->long_name : "");
    if (!codec) {
        fprintf(stderr, "FFmpeg decoder: could not find H264 decoder\n");
        return -1;
    }

    ctx->avctx = avcodec_alloc_context3(codec);
    if (!ctx->avctx) {
        ffmpeg_decoder_destroy(ctx);
        return -1;
    }

    ctx->pkt = av_packet_alloc();
    if (!ctx->pkt) {
        ffmpeg_decoder_destroy(ctx);
        return -1;
    }

    ctx->frame = av_frame_alloc();
    if (!ctx->frame) {
        ffmpeg_decoder_destroy(ctx);
        return -1;
    }

    ctx->parser = av_parser_init(codec->id);

    AVDictionary *opts = NULL;
    bool requestedVitaLowDelay = false;
    enum AVDiscard targetSkipLoopFilter = AVDISCARD_DEFAULT;
    enum AVDiscard targetSkipIdct = AVDISCARD_DEFAULT;
    enum AVDiscard targetSkipFrame = AVDISCARD_DEFAULT;

#ifdef BOREALIS_USE_GXM
    if (codec->id == AV_CODEC_ID_H264) {
        int requestedPixelMode = g_video_settings_snapshot.pixel_format_mode;
        ctx->is_vita_hw = (strcmp(codec->name, "h264_vita") == 0);
        
        // Clean performance baseline: force YUV/NV12 decode path on Vita FFmpeg.
        // RGBA decode path is currently much slower and has shown instability.
        // For h264_vita hardware decoder, natively use AV_PIX_FMT_VITA_NV12 to enable Direct Rendering (zero CPU copy).
        enum AVPixelFormat requestedPixFmt = ctx->is_vita_hw ? AV_PIX_FMT_VITA_NV12 : AV_PIX_FMT_YUV420P;
        ctx->avctx->pix_fmt = requestedPixFmt;
        
        VITA_DEBUG_LOG("[FFMPEG] Requested pixel format mode=%d, using pix_fmt=%d (is_vita_hw=%d)", 
                       requestedPixelMode, (int)requestedPixFmt, ctx->is_vita_hw ? 1 : 0);

        // Enable direct rendering for Vita hardware decoder to achieve maximum performance (60/60 fps)
        ctx->use_direct_render = ctx->is_vita_hw;
        if (ctx->use_direct_render) {
            ctx->avctx->get_buffer2 = get_buffer2_direct;
            av_dict_set(&opts, "vita_h264_dr", "1", 0);
            av_dict_set(&opts, "vita_h264_low_delay_mode", "low", 0);
            requestedVitaLowDelay = true;
            VITA_DEBUG_LOG("[FFMPEG] Direct render enabled for h264_vita! Assigned get_buffer2_direct and set vita_h264_dr=1");
        } else {
            VITA_DEBUG_LOG("[FFMPEG] Direct render disabled; software upload path enabled");
        }

        if (ctx->is_vita_hw) {
            VITA_DEBUG_LOG("[FFMPEG] Vita hardware codec detected: %s", codec->name);
        }
    VITA_DEBUG_LOG("[FFMPEG] ffmpeg_decoder_init: H264 on GXM: direct_render=%d", ctx->use_direct_render ? 1 : 0);
    // Force low-delay decoding: prevent the decoder from buffering frames internally.
    // Without this, h264_vita retains 1 frame before outputting, adding ~16.67ms of latency
    // that Legacy (direct SceAvcdec) doesn't have.
    ctx->avctx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    // Set conservative fixed defaults for the Vita: slice threading and skip loop filter.
    // We try 2 threads by default for better throughput for SW decoding.
    // Hardware decoding (h264_vita) is single-threaded; setting thread_count > 1 leads to severe instability and crashes.
    ctx->avctx->thread_count = ctx->is_vita_hw ? 1 : 2;
    ctx->avctx->thread_type = ctx->is_vita_hw ? 0 : FF_THREAD_SLICE;
    // Set refcounted_frames via options so we don't depend on direct field presence in headers
    av_dict_set(&opts, "refcounted_frames", "1", 0);
    // Open with conservative defaults for maximum compatibility on h264_vita.
    // We apply aggressive discard values after successful avcodec_open2.
    ctx->avctx->skip_loop_filter = AVDISCARD_DEFAULT;
    ctx->avctx->skip_idct = AVDISCARD_DEFAULT;
    ctx->avctx->skip_frame = AVDISCARD_DEFAULT;
    targetSkipLoopFilter = AVDISCARD_NONREF;
    targetSkipIdct = AVDISCARD_NONREF;
    targetSkipFrame = AVDISCARD_DEFAULT;

    // GPU-YUV fast path shifts the bottleneck to decode throughput.
    // Prefer throughput on Vita by default; env overrides still apply.
    if (requestedPixelMode == 1) {
        targetSkipLoopFilter = AVDISCARD_ALL;
        targetSkipIdct = AVDISCARD_ALL;
        targetSkipFrame = AVDISCARD_DEFAULT; // Do NOT discard B-frames (allow stable 60/60 fps)
        ctx->avctx->flags2 |= AV_CODEC_FLAG2_FAST;
        VITA_DEBUG_LOG("[FFMPEG] perf profile: YUV GPU mode -> default skip_loop_filter=ALL skip_idct=ALL skip_frame=DEFAULT flags2|=FAST");

        // Hardware h264_vita must remain single-threaded.
        if (ctx->is_vita_hw) {
            ctx->avctx->thread_count = 1;
            VITA_DEBUG_LOG("[FFMPEG] perf profile: YUV GPU + h264_vita -> thread_count forced to 1 for stability");
        }
    }
    int refcounted_frames_val = 1;
        // Allow runtime override via environment variables for testing/diagnosis
        const char* threadCountEnv = getenv("MOONLIGHT_FFMPEG_THREAD_COUNT");
        if (threadCountEnv) {
            int tcount = atoi(threadCountEnv);
            if (tcount > 0) ctx->avctx->thread_count = tcount;
        }
        const char* threadTypeEnv = getenv("MOONLIGHT_FFMPEG_THREAD_TYPE");
        if (threadTypeEnv) {
            if (strcmp(threadTypeEnv, "slice") == 0) ctx->avctx->thread_type = FF_THREAD_SLICE;
            else if (strcmp(threadTypeEnv, "frame") == 0) ctx->avctx->thread_type = FF_THREAD_FRAME;
        }
        const char* skipLoopEnv = getenv("MOONLIGHT_FFMPEG_SKIP_LOOP_FILTER");
        if (skipLoopEnv) {
            if (strcmp(skipLoopEnv, "all") == 0) targetSkipLoopFilter = AVDISCARD_ALL;
            else if (strcmp(skipLoopEnv, "nonref") == 0) targetSkipLoopFilter = AVDISCARD_NONREF;
            else if (strcmp(skipLoopEnv, "none") == 0) targetSkipLoopFilter = AVDISCARD_DEFAULT;
        }
        const char* skipIdctEnv = getenv("MOONLIGHT_FFMPEG_SKIP_IDCT");
        if (skipIdctEnv) {
            if (strcmp(skipIdctEnv, "all") == 0) targetSkipIdct = AVDISCARD_ALL;
            else if (strcmp(skipIdctEnv, "nonref") == 0) targetSkipIdct = AVDISCARD_NONREF;
            else if (strcmp(skipIdctEnv, "none") == 0) targetSkipIdct = AVDISCARD_DEFAULT;
        }
        const char* skipFrameEnv = getenv("MOONLIGHT_FFMPEG_SKIP_FRAME");
        if (skipFrameEnv) {
            if (strcmp(skipFrameEnv, "all") == 0) targetSkipFrame = AVDISCARD_ALL;
            else if (strcmp(skipFrameEnv, "nonref") == 0) targetSkipFrame = AVDISCARD_NONREF;
            else if (strcmp(skipFrameEnv, "bidir") == 0) targetSkipFrame = AVDISCARD_BIDIR;
            else if (strcmp(skipFrameEnv, "none") == 0) targetSkipFrame = AVDISCARD_DEFAULT;
        }
    VITA_DEBUG_LOG("[FFMPEG] ffmpeg_decoder_init: thread_count=%d thread_type=%d refcounted=%d skip_loop_filter(open)=%d target_skip_loop_filter=%d target_skip_idct=%d target_skip_frame=%d flags=0x%X flags2=0x%X", ctx->avctx->thread_count, ctx->avctx->thread_type, refcounted_frames_val, ctx->avctx->skip_loop_filter, targetSkipLoopFilter, targetSkipIdct, targetSkipFrame, (unsigned)ctx->avctx->flags, (unsigned)ctx->avctx->flags2);
    } else {
        // Non-H264 codecs keep default behavior.
        ctx->use_direct_render = false;
    }
#else
    // Force output pixel format to YUV420P
    ctx->avctx->pix_fmt = (g_video_settings_snapshot.pixel_format_mode == 0) ? AV_PIX_FMT_RGBA : AV_PIX_FMT_YUV420P;
    ctx->use_direct_render = false;
#endif

    int openRet = avcodec_open2(ctx->avctx, codec, &opts);
    if (requestedVitaLowDelay) {
        bool accepted = av_dict_get(opts, "vita_h264_low_delay_mode", nullptr, 0) == nullptr;
        VITA_DEBUG_LOG("[FFMPEG] vita_h264_low_delay_mode=low %s",
                       accepted ? "accepted" : "not supported by installed FFmpeg-vita");
    }
    av_dict_free(&opts);
    if (openRet < 0) {
#ifdef BOREALIS_USE_GXM
        if (ctx->is_vita_hw && ctx->avctx->skip_loop_filter != AVDISCARD_DEFAULT) {
            VITA_DEBUG_LOG("[FFMPEG] avcodec_open2 failed with skip_loop_filter=%d, retrying with default", ctx->avctx->skip_loop_filter);
            // Recreate a clean codec context before retry; some Vita builds don't
            // reliably support reopening after a failed avcodec_open2 on the same avctx.
            int savedThreadCount = ctx->avctx->thread_count;
            int savedThreadType = ctx->avctx->thread_type;
            enum AVPixelFormat savedPixFmt = ctx->avctx->pix_fmt;

            avcodec_free_context(&ctx->avctx);
            ctx->avctx = avcodec_alloc_context3(codec);
            if (ctx->avctx) {
                ctx->avctx->pix_fmt = savedPixFmt;
                ctx->avctx->thread_count = savedThreadCount;
                ctx->avctx->thread_type = savedThreadType;
                ctx->avctx->skip_loop_filter = AVDISCARD_DEFAULT;

                AVDictionary* retryOpts = nullptr;
                av_dict_set(&retryOpts, "refcounted_frames", "1", 0);
                openRet = avcodec_open2(ctx->avctx, codec, &retryOpts);
                av_dict_free(&retryOpts);
            }

            // If still failing with multiple threads, retry once with single-thread init.
            if (openRet < 0 && ctx->avctx && savedThreadCount > 1) {
                VITA_DEBUG_LOG("[FFMPEG] avcodec_open2 still failing with threads=%d, retrying with threads=1", savedThreadCount);
                avcodec_free_context(&ctx->avctx);
                ctx->avctx = avcodec_alloc_context3(codec);
                if (ctx->avctx) {
                    ctx->avctx->pix_fmt = savedPixFmt;
                    ctx->avctx->thread_count = 1;
                    ctx->avctx->thread_type = savedThreadType;
                    ctx->avctx->skip_loop_filter = AVDISCARD_DEFAULT;

                    AVDictionary* retrySingleThreadOpts = nullptr;
                    av_dict_set(&retrySingleThreadOpts, "refcounted_frames", "1", 0);
                    openRet = avcodec_open2(ctx->avctx, codec, &retrySingleThreadOpts);
                    av_dict_free(&retrySingleThreadOpts);
                }
            }
        }
#endif
        if (openRet < 0) {
            ffmpeg_decoder_destroy(ctx);
            fprintf(stderr, "FFmpeg decoder: failed to open codec %s\n", codec->name);
            return -1;
        }
    }

    // Apply decode-speed tuning after open to avoid h264_vita init failures.
    ctx->avctx->skip_loop_filter = targetSkipLoopFilter;
    ctx->avctx->skip_idct = targetSkipIdct;
    ctx->avctx->skip_frame = targetSkipFrame;
    VITA_DEBUG_LOG("[FFMPEG] ffmpeg_decoder_init: post-open tuning skip_loop_filter=%d skip_idct=%d skip_frame=%d", ctx->avctx->skip_loop_filter, ctx->avctx->skip_idct, ctx->avctx->skip_frame);

    ctx->initialized = 1;
    // Runtime diagnostics: report codec and hw acceleration info
    VITA_DEBUG_LOG("[FFMPEG] codec=%s avctx->get_buffer2=%p avctx->hw_device_ctx=%p use_direct_render=%d", codec->name, (void*)ctx->avctx->get_buffer2, (void*)ctx->avctx->hw_device_ctx, ctx->use_direct_render);
    return 0;
}

void ffmpeg_decoder_destroy(FFmpegDecoderContext *ctx)
{
    if (!ctx) {
        return;
    }
    if (ctx->avctx) {
        avcodec_free_context(&ctx->avctx);
    }
    if (ctx->parser) {
        av_parser_close(ctx->parser);
        ctx->parser = NULL;
    }
    if (ctx->frame) {
        av_frame_free(&ctx->frame);
    }
    if (ctx->pkt) {
        av_packet_free(&ctx->pkt);
    }
    memset(ctx, 0, sizeof(*ctx));
}

int ffmpeg_decoder_decode(FFmpegDecoderContext *ctx, const uint8_t *data, int size)
{
    if (!ctx || !ctx->initialized || !data || size <= 0) {
        return AVERROR(EINVAL);
    }

    if (av_new_packet(ctx->pkt, size) < 0) {
        return AVERROR(ENOMEM);
    }

    memcpy(ctx->pkt->data, data, (size_t)size);

    int ret = avcodec_send_packet(ctx->avctx, ctx->pkt);
    av_packet_unref(ctx->pkt);
    return ret;
}
