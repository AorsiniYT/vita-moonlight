#include "ffmpeg/modules/ffmpeg_decoder.hpp"
#include "../ffmpeg.hpp"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

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

    const AVCodec *codec = avcodec_find_decoder_by_name("h264_vita");
    if (!codec) {
        codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    }

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

#ifdef BOREALIS_USE_GXM
    if (codec->name && strcmp(codec->name, "h264_vita") == 0) {
        av_dict_set(&opts, "vita_h264_dr", "1", 0);
        ctx->avctx->get_buffer2 = get_buffer2_direct;
    }
#endif

    // Force single-threaded decoding to avoid libavcodec threading issues on Vita
    av_dict_set(&opts, "threads", "1", 0);
    ctx->avctx->thread_count = 1;

    if (avcodec_open2(ctx->avctx, codec, &opts) < 0) {
        av_dict_free(&opts);
        ffmpeg_decoder_destroy(ctx);
        fprintf(stderr, "FFmpeg decoder: failed to open codec %s\n", codec->name);
        return -1;
    }

    av_dict_free(&opts);

    ctx->initialized = 1;
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
