/*
 * Copyright (c) 2014 Andy Huang <andyspider@126.com>
 *
 * This file is part of Camkit.
 *
 * Camkit is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Camkit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Camkit; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ffmpeg_common.h"
#include "camkit/encode.h"

struct enc_handle
{
	AVCodec *codec;
	AVCodecContext *ctx;
	AVFrame *frame;
	uint8_t *inbuffer;
	int inbufsize;
	AVPacket packet;
	unsigned long frame_counter;

	struct enc_param params;
};

struct enc_handle *encode_open(struct enc_param param)
{
	struct enc_handle *handle = malloc(sizeof(struct enc_handle));
	if (!handle)
	{
		printf("--- malloc enc handle failed\n");
		return NULL;
	}

	CLEAR(*handle);
	handle->codec = NULL;
	handle->ctx = NULL;
	handle->frame = NULL;
	handle->inbuffer = NULL;
	handle->inbufsize = 0;
	handle->frame_counter = 0;
	handle->params.src_picwidth = param.src_picwidth;
	handle->params.src_picheight = param.src_picheight;
	handle->params.enc_picwidth = param.enc_picwidth;
	handle->params.enc_picheight = param.enc_picheight;
	handle->params.fps = param.fps;
	handle->params.bitrate = param.bitrate;
	handle->params.gop = param.gop;
	handle->params.chroma_interleave = param.chroma_interleave;

	avcodec_register_all();
	handle->codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!handle->codec)
	{
		printf("--- H264 codec not found\n");
		goto err0;
	}

	handle->ctx = avcodec_alloc_context3(handle->codec);
	if (!handle->ctx)
	{
		printf("--- Could not allocate video codec context\n");
		goto err0;
	}

	handle->ctx->bit_rate = handle->params.bitrate * 1000;    // to kbps
	handle->ctx->width = handle->params.src_picwidth;
	handle->ctx->height = handle->params.src_picheight;
	handle->ctx->time_base = (AVRational
			)
			{ 1, handle->params.fps };    // frames per second
	handle->ctx->gop_size = handle->params.gop;
	handle->ctx->max_b_frames = 1;
	handle->ctx->pix_fmt = AV_PIX_FMT_YUV420P;
//	handle->ctx->thread_count = 1;
	// eliminate frame delay!
	av_opt_set(handle->ctx->priv_data, "preset", "ultrafast", 0);
	av_opt_set(handle->ctx->priv_data, "tune", "zerolatency", 0);
	av_opt_set(handle->ctx->priv_data, "x264opts",
			"no-mbtree:sliced-threads:sync-lookahead=0", 0);

	if (avcodec_open2(handle->ctx, handle->codec, NULL) < 0)
	{
		printf("--- Could not open codec\n");
		goto err1;
	}

	handle->frame = av_frame_alloc();
	if (!handle->frame)
	{
		printf("--- Could not allocate video frame\n");
		goto err2;
	}

	handle->frame->format = handle->ctx->pix_fmt;
	handle->frame->width = handle->ctx->width;
	handle->frame->height = handle->ctx->height;
	handle->inbufsize = avpicture_get_size(AV_PIX_FMT_YUV420P,
			handle->params.src_picwidth, handle->params.src_picheight);
	handle->inbuffer = av_malloc(handle->inbufsize);
	if (!handle->inbuffer)
	{
		printf("--- Could not allocate inbuffer\n");
		goto err3;
	}
	avpicture_fill((AVPicture *) handle->frame, handle->inbuffer,
			AV_PIX_FMT_YUV420P, handle->params.src_picwidth,
			handle->params.src_picheight);

	av_init_packet(&handle->packet);
	handle->packet.data = NULL;
	handle->packet.size = 0;

	printf("+++ Encode Opened\n");
	return handle;

	err3: av_frame_free(&handle->frame);
	err2: avcodec_close(handle->ctx);
	err1: av_free(handle->ctx);
	err0: free(handle);
	return NULL;
}

void encode_close(struct enc_handle *handle)
{
	av_free_packet(&handle->packet);
	av_free(handle->inbuffer);
	av_frame_free(&handle->frame);
	avcodec_close(handle->ctx);
	av_free(handle->ctx);
	free(handle);
	printf("+++ Encode Closed\n");
}

int encode_do(struct enc_handle *handle, void *ibuf, int ilen, void **pobuf,
		int *polen, enum pic_t *type)
{
	int got_output, ret;

	// reinit pkt
	av_free_packet(&handle->packet);
	av_init_packet(&handle->packet);
	handle->packet.data = NULL;
	handle->packet.size = 0;

	assert(handle->inbufsize == ilen);
	memcpy(handle->inbuffer, ibuf, ilen);
	handle->frame->pts = handle->frame_counter++;

	ret = avcodec_encode_video2(handle->ctx, &handle->packet, handle->frame,
			&got_output);
	// cancel key frame
	handle->frame->pict_type = 0;
	handle->frame->key_frame = 0;
	if (ret < 0)
	{
		printf("--- Error encoding frame\n");
		return -1;
	}

	if (got_output)
	{
		*pobuf = handle->packet.data;
		*polen = handle->packet.size;
		switch (handle->ctx->coded_frame->pict_type)
		{
			case AV_PICTURE_TYPE_I:
				*type = I;
				break;
			case AV_PICTURE_TYPE_P:
				*type = P;
				break;
			case AV_PICTURE_TYPE_B:
				*type = B;
				break;
			default:
				*type = NONE;
				break;
		}
	}
	else	// get the delayed frame
	{
		printf("!!! encoded frame delayed!\n");
		*pobuf = NULL;
		*polen = 0;
		*type = NONE;
	}

	return 0;
}

int encode_get_headers(struct enc_handle *handle, void **pbuf, int *plen,
		enum pic_t *type)
{
	*pbuf = NULL;
	*plen = 0;
	*type = NONE;

	return 0;
}

int encode_set_qp(struct enc_handle *handle, int val)
{
	UNUSED(handle);
	UNUSED(val);
	printf("*** %s.%s: This function is not implemented\n", __FILE__,
			__FUNCTION__);
	return -1;
}

int encode_set_bitrate(struct enc_handle *handle, int val)
{
	handle->ctx->bit_rate = val;
	return 0;
}

int encode_set_framerate(struct enc_handle *handle, int val)
{
	handle->ctx->time_base = (AVRational
			)
			{ 1, val };
	return 0;
}

void encode_force_Ipic(struct enc_handle *handle)
{
	handle->frame->pict_type = AV_PICTURE_TYPE_I;
	handle->frame->key_frame = 1;

	return;
}
