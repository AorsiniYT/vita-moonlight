#include "ffmpeg_decoder.hpp"
#include "ffmpeg.hpp"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

/* FFmpeg headers: wrap with C++ guard so this C file also compiles when
    included from C++ translation units. */
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
/* Declare get_buffer2_direct from ffmpeg.cpp (implemented in C++ file).
    Use plain declaration here so the C compiler sees it only when needed. */
/* get_buffer2_direct is implemented in a C++ file; ensure C linkage at import point */
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
    memset(ctx, 0, sizeof(*ctx));

    // Try to find the h264_vita decoder first
    const AVCodec *codec = avcodec_find_decoder_by_name("h264_vita");
    if (!codec) {
        // fallback to generic h264
        codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    }
        #ifndef BOREALIS_USE_GXM
        #endif

    if (!codec) {
        fprintf(stderr, "FFmpeg decoder: could not find H264 decoder\n");
        return -1;
    }
            /* avcodec_register_all() is deprecated/removed in newer FFmpeg versions
               and can cause link-time undefined references. Rely on modern
               libavcodec initialization (no-op here). */
    ctx->avctx = avcodec_alloc_context3(codec);
    if (!ctx->avctx) return -1;

    // If this is the vita-specific decoder, enable DR option and allocator
#ifdef BOREALIS_USE_GXM
    if (strcmp(codec->name, "h264_vita") == 0) {
        AVDictionary *opts = NULL;
        av_dict_set(&opts, "vita_h264_dr", "1", 0);
        // Assign custom allocator
        ctx->avctx->get_buffer2 = get_buffer2_direct;
        if (avcodec_open2(ctx->avctx, codec, &opts) < 0) {
            av_dict_free(&opts);
            fprintf(stderr, "FFmpeg decoder: failed to open h264_vita\n");
            return -1;
        }
        av_dict_free(&opts);
    } else
#endif
    {
        if (avcodec_open2(ctx->avctx, codec, NULL) < 0) {
            fprintf(stderr, "FFmpeg decoder: failed to open codec\n");
            return -1;
        }
    }

    ctx->parser = av_parser_init(AV_CODEC_ID_H264);
    ctx->frame = av_frame_alloc();
    ctx->pkt = av_packet_alloc();
    ctx->initialized = 1;
    return 0;
}

void ffmpeg_decoder_destroy(FFmpegDecoderContext *ctx)
{
    if (!ctx) return;
    if (ctx->avctx) { avcodec_free_context(&ctx->avctx); }
    if (ctx->parser) { av_parser_close(ctx->parser); }
    if (ctx->frame) { av_frame_free(&ctx->frame); }
    if (ctx->pkt) { av_packet_free(&ctx->pkt); }
    memset(ctx,0,sizeof(*ctx));
}

int ffmpeg_decoder_decode(FFmpegDecoderContext *ctx, const uint8_t *data, int size)
{
    if (!ctx || !ctx->initialized) return -1;
    // Very small stub: feed packet and try to receive a frame once
    int ret = av_packet_from_data(ctx->pkt, (uint8_t*)data, size);
    if (ret < 0) return -1;
    ret = avcodec_send_packet(ctx->avctx, ctx->pkt);
    if (ret < 0) return -1;
    ret = avcodec_receive_frame(ctx->avctx, ctx->frame);
    if (ret == 0) {
        // frame available: real code should attach to dr_texture and hand it to renderer
        av_frame_unref(ctx->frame);
        av_packet_unref(ctx->pkt);
        return 1;
    }
    av_packet_unref(ctx->pkt);
    return 0; // need more data or no frame
}
