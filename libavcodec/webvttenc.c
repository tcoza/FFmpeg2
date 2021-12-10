/*
 * WebVTT subtitle encoder
 * Copyright (c) 2010  Aurelien Jacobs <aurel@gnuage.org>
 * Copyright (c) 2014  Aman Gupta <ffmpeg@tmm1.net>
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

#include <stdarg.h>
#include "avcodec.h"
#include "encode.h"
#include "libavutil/avstring.h"
#include "libavutil/bprint.h"
#include "libavutil/ass_split_internal.h"
#include "libavutil/ass_internal.h"
#include "internal.h"

#define WEBVTT_STACK_SIZE 64
typedef struct {
    AVCodecContext *avctx;
    ASSSplitContext *ass_ctx;
    int is_default_ass_context;
    AVBPrint buffer;
    unsigned timestamp_end;
    int count;
    char stack[WEBVTT_STACK_SIZE];
    int stack_ptr;
} WebVTTContext;

#ifdef __GNUC__
__attribute__ ((__format__ (__printf__, 2, 3)))
#endif
static void webvtt_print(WebVTTContext *s, const char *str, ...)
{
    va_list vargs;
    va_start(vargs, str);
    av_vbprintf(&s->buffer, str, vargs);
    va_end(vargs);
}

static int webvtt_stack_push(WebVTTContext *s, const char c)
{
    if (s->stack_ptr >= WEBVTT_STACK_SIZE)
        return -1;
    s->stack[s->stack_ptr++] = c;
    return 0;
}

static char webvtt_stack_pop(WebVTTContext *s)
{
    if (s->stack_ptr <= 0)
        return 0;
    return s->stack[--s->stack_ptr];
}

static int webvtt_stack_find(WebVTTContext *s, const char c)
{
    int i;
    for (i = s->stack_ptr-1; i >= 0; i--)
        if (s->stack[i] == c)
            break;
    return i;
}

static void webvtt_close_tag(WebVTTContext *s, char tag)
{
    webvtt_print(s, "</%c>", tag);
}

static void webvtt_stack_push_pop(WebVTTContext *s, const char c, int close)
{
    if (close) {
        int i = c ? webvtt_stack_find(s, c) : 0;
        if (i < 0)
            return;
        while (s->stack_ptr != i)
            webvtt_close_tag(s, webvtt_stack_pop(s));
    } else if (webvtt_stack_push(s, c) < 0)
        av_log(s->avctx, AV_LOG_ERROR, "tag stack overflow\n");
}

static void webvtt_style_apply(WebVTTContext *s, const char *style)
{
    ASSStyle *st = avpriv_ass_style_get(s->ass_ctx, style);
    if (st) {
        if (st->bold != ASS_DEFAULT_BOLD) {
            webvtt_print(s, "<b>");
            webvtt_stack_push(s, 'b');
        }
        if (st->italic != ASS_DEFAULT_ITALIC) {
            webvtt_print(s, "<i>");
            webvtt_stack_push(s, 'i');
        }
        if (st->underline != ASS_DEFAULT_UNDERLINE) {
            webvtt_print(s, "<u>");
            webvtt_stack_push(s, 'u');
        }
    }
}

static void webvtt_text_cb(void *priv, const char *text, int len)
{
    WebVTTContext *s = priv;
    av_bprint_append_data(&s->buffer, text, len);
}

static void webvtt_new_line_cb(void *priv, int forced)
{
    webvtt_print(priv, "\n");
}

static void webvtt_hard_space_cb(void *priv)
{
    webvtt_print(priv, "&nbsp;");
}

static void webvtt_style_cb(void *priv, char style, int close)
{
    if (style == 's') // strikethrough unsupported
        return;

    webvtt_stack_push_pop(priv, style, close);
    if (!close)
        webvtt_print(priv, "<%c>", style);
}

static void webvtt_cancel_overrides_cb(void *priv, const char *style)
{
    webvtt_stack_push_pop(priv, 0, 1);
    webvtt_style_apply(priv, style);
}

static void webvtt_end_cb(void *priv)
{
    webvtt_stack_push_pop(priv, 0, 1);
}

static const ASSCodesCallbacks webvtt_callbacks = {
    .text             = webvtt_text_cb,
    .new_line         = webvtt_new_line_cb,
    .hard_space       = webvtt_hard_space_cb,
    .style            = webvtt_style_cb,
    .color            = NULL,
    .font_name        = NULL,
    .font_size        = NULL,
    .alignment        = NULL,
    .cancel_overrides = webvtt_cancel_overrides_cb,
    .move             = NULL,
    .end              = webvtt_end_cb,
};

static void ensure_ass_context(WebVTTContext* s, const AVFrame* frame)
{
    if (s->ass_ctx && !s->is_default_ass_context)
        // We already have a (non-default context)
        return;

    if (!frame->num_subtitle_areas)
        // Don't need ass context for processing empty subtitle frames
        return;

    // The frame has content, so we need to set up a context
    if (frame->subtitle_header && frame->subtitle_header->size > 0) {
        avpriv_ass_split_free(s->ass_ctx);
        const char* subtitle_header = (char*)frame->subtitle_header->data;
        s->ass_ctx = avpriv_ass_split(subtitle_header);
        s->is_default_ass_context = 0;
    }
    else if (!s->ass_ctx) {
        char* subtitle_header = avpriv_ass_get_subtitle_header_default(0);
        if (!subtitle_header)
            return;

        s->ass_ctx = avpriv_ass_split(subtitle_header);
        s->is_default_ass_context = 1;
        av_free(subtitle_header);
    }
}

static int webvtt_encode_frame(AVCodecContext* avctx, AVPacket* avpkt,
                               const AVFrame* frame, int* got_packet)
{
    WebVTTContext *s = avctx->priv_data;
    ASSDialog *dialog;
    int ret, i;

    ensure_ass_context(s, frame);

    av_bprint_clear(&s->buffer);

    for (i=0; i< frame->num_subtitle_areas; i++) {
        const char *ass = frame->subtitle_areas[i]->ass;

        if (frame->subtitle_areas[i]->type != AV_SUBTITLE_FMT_ASS) {
            av_log(avctx, AV_LOG_ERROR, "Only AV_SUBTITLE_FMT_ASS type supported.\n");
            return AVERROR(EINVAL);
        }

        if (ass) {
            dialog = avpriv_ass_split_dialog(s->ass_ctx, ass);
            if (!dialog)
                return AVERROR(ENOMEM);
            webvtt_style_apply(s, dialog->style);
            avpriv_ass_split_override_codes(&webvtt_callbacks, s, dialog->text);
            avpriv_ass_free_dialog(&dialog);
        }
    }

    if (!av_bprint_is_complete(&s->buffer))
        return AVERROR(ENOMEM);

    ret = ff_get_encode_buffer(avctx, avpkt, s->buffer.len + 1, 0);
    if (ret < 0) {
        av_log(avctx, AV_LOG_ERROR, "Error getting output packet.\n");
        return ret;
    }

    memcpy(avpkt->data, s->buffer.str, s->buffer.len);
    avpkt->size = s->buffer.len;
    *got_packet = s->buffer.len > 0;

    return 0;
}

static int webvtt_encode_close(AVCodecContext *avctx)
{
    WebVTTContext *s = avctx->priv_data;
    avpriv_ass_split_free(s->ass_ctx);
    av_bprint_finalize(&s->buffer, NULL);
    return 0;
}

static av_cold int webvtt_encode_init(AVCodecContext *avctx)
{
    WebVTTContext *s = avctx->priv_data;
    s->avctx = avctx;
    s->ass_ctx = avpriv_ass_split((char*)avctx->subtitle_header);
    av_bprint_init(&s->buffer, 0, AV_BPRINT_SIZE_UNLIMITED);
    return 0;
}

const AVCodec ff_webvtt_encoder = {
    .name           = "webvtt",
    .long_name      = NULL_IF_CONFIG_SMALL("WebVTT subtitle"),
    .type           = AVMEDIA_TYPE_SUBTITLE,
    .id             = AV_CODEC_ID_WEBVTT,
    .priv_data_size = sizeof(WebVTTContext),
    .init           = webvtt_encode_init,
    .encode2        = webvtt_encode_frame,
    .close          = webvtt_encode_close,
    .caps_internal  = FF_CODEC_CAP_INIT_THREADSAFE,
};
