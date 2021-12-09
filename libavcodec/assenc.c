/*
 * SSA/ASS encoder
 * Copyright (c) 2010  Aurelien Jacobs <aurel@gnuage.org>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <string.h>

#include "avcodec.h"
#include "encode.h"
#include "libavutil/ass_internal.h"
#include "internal.h"
#include "libavutil/avstring.h"
#include "libavutil/internal.h"
#include "libavutil/mem.h"

static void check_write_header(AVCodecContext* avctx, const AVFrame* frame)
{
    if (avctx->extradata_size)
        return;

    if (frame->subtitle_header && frame->subtitle_header->size > 0) {
        const char* subtitle_header = (char*)frame->subtitle_header->data;
        avctx->extradata_size = strlen(subtitle_header);
        avctx->extradata = av_mallocz(frame->subtitle_header->size + 1);
        memcpy(avctx->extradata, subtitle_header, avctx->extradata_size);
        avctx->extradata[avctx->extradata_size] = 0;
    }

    if (!avctx->extradata_size) {
        const char* subtitle_header = avpriv_ass_get_subtitle_header_default(0);
        if (!subtitle_header)
            return;

        avctx->extradata_size = strlen(subtitle_header);
        avctx->extradata = av_mallocz(avctx->extradata_size + 1);
        memcpy(avctx->extradata, subtitle_header, avctx->extradata_size);
        avctx->extradata[avctx->extradata_size] = 0;
        av_freep(&subtitle_header);
    }
}

static av_cold int ass_encode_init(AVCodecContext *avctx)
{
    if (avctx->subtitle_header_size) {
        avctx->extradata = av_malloc(avctx->subtitle_header_size + 1);
        if (!avctx->extradata)
            return AVERROR(ENOMEM);
        memcpy(avctx->extradata, avctx->subtitle_header, avctx->subtitle_header_size);
        avctx->extradata_size                   = avctx->subtitle_header_size;
        avctx->extradata[avctx->extradata_size] = 0;
    }

    return 0;
}

static int ass_encode_frame(AVCodecContext* avctx, AVPacket* avpkt,
                            const AVFrame* frame, int* got_packet)
{
    int ret;
    size_t req_len = 0, total_len = 0;

    check_write_header(avctx, frame);

    for (unsigned i = 0; i < frame->num_subtitle_areas; i++) {
        const char *ass = frame->subtitle_areas[i]->ass;

        if (frame->subtitle_areas[i]->type != AV_SUBTITLE_FMT_ASS) {
            av_log(avctx, AV_LOG_ERROR, "Only AV_SUBTITLE_FMT_ASS type supported.\n");
            return AVERROR(EINVAL);
        }

        if (ass)
            req_len += strlen(ass);
    }

    ret = ff_get_encode_buffer(avctx, avpkt, req_len + 1, 0);
    if (ret < 0) {
        av_log(avctx, AV_LOG_ERROR, "Error getting output packet.\n");
        return ret;
    }

    for (unsigned i = 0; i < frame->num_subtitle_areas; i++) {
        const char *ass = frame->subtitle_areas[i]->ass;

        if (ass) {
            size_t len = av_strlcpy((char *)avpkt->data + total_len, ass, avpkt->size - total_len);
            total_len += len;
        }
    }

    avpkt->size = total_len;
    *got_packet = total_len > 0;

    return 0;
}

#if CONFIG_SSA_ENCODER
const AVCodec ff_ssa_encoder = {
    .name         = "ssa",
    .long_name    = NULL_IF_CONFIG_SMALL("ASS (Advanced SubStation Alpha) subtitle"),
    .type         = AVMEDIA_TYPE_SUBTITLE,
    .id           = AV_CODEC_ID_ASS,
    .init         = ass_encode_init,
    .encode2      = ass_encode_frame,
    .caps_internal  = FF_CODEC_CAP_INIT_THREADSAFE,
};
#endif

#if CONFIG_ASS_ENCODER
const AVCodec ff_ass_encoder = {
    .name         = "ass",
    .long_name    = NULL_IF_CONFIG_SMALL("ASS (Advanced SubStation Alpha) subtitle"),
    .type         = AVMEDIA_TYPE_SUBTITLE,
    .id           = AV_CODEC_ID_ASS,
    .init         = ass_encode_init,
    .encode2      = ass_encode_frame,
    .caps_internal  = FF_CODEC_CAP_INIT_THREADSAFE,
};
#endif
