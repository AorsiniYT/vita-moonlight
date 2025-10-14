#include "ffmpeg/modules/ffmpeg_decoder.hpp"
#include "../ffmpeg.hpp"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

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
    if (!ctx->avctx) return -1;

#ifdef BOREALIS_USE_GXM
    if (strcmp(codec->name, "h264_vita") == 0) {
        AVDictionary *opts = NULL;
        /* Enable vita direct-render for h264_vita, but force single-threaded
         * decoding to avoid races / memory corruption exposed by multithreaded
         * decoders on Vita (mitigation used by reference projects).
         */
        av_dict_set(&opts, "vita_h264_dr", "1", 0);
        av_dict_set(&opts, "threads", "1", 0);
        ctx->avctx->thread_count = 1;
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
        /* Force single-threaded decoding by default as a mitigation for
         * unexplained crashes when using multithreaded libavcodec on Vita.
         * This mirrors the approach used in the reference `wiliwili` code
         * (limiting lavc threads) and reduces concurrency surface.
         */
        AVDictionary *opts = NULL;
        av_dict_set(&opts, "threads", "1", 0);
        ctx->avctx->thread_count = 1;
        if (avcodec_open2(ctx->avctx, codec, &opts) < 0) {
            av_dict_free(&opts);
            fprintf(stderr, "FFmpeg decoder: failed to open codec\n");
            return -1;
        }
        av_dict_free(&opts);
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
    int ret = av_packet_from_data(ctx->pkt, (uint8_t*)data, size);
    if (ret < 0) return -1;
    ret = avcodec_send_packet(ctx->avctx, ctx->pkt);
    if (ret < 0) return -1;
    ret = avcodec_receive_frame(ctx->avctx, ctx->frame);
    if (ret == 0) {
        av_frame_unref(ctx->frame);
        av_packet_unref(ctx->pkt);
        return 1;
    }
    av_packet_unref(ctx->pkt);
    return 0;
}
