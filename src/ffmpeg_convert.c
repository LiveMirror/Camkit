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
#include <linux/videodev2.h>
#include <libswscale/swscale.h>
#include "ffmpeg_common.h"
#include "camkit/convert.h"

struct cvt_handle
{
	struct SwsContext *sws_ctx;
	uint8_t *src_buffer;
	int src_buffersize;
	AVFrame *src_frame;
	uint8_t *dst_buffer;
	int dst_buffersize;
	AVFrame *dst_frame;
	enum AVPixelFormat inavfmt;
	enum AVPixelFormat outavfmt;

	struct cvt_param params;
};

static enum AVPixelFormat v4lFmt2AVFmt(U32 v4lfmt)
{
	enum AVPixelFormat avfmt = AV_PIX_FMT_NONE;

	switch (v4lfmt)
	{
		case V4L2_PIX_FMT_YUV420:
			avfmt = AV_PIX_FMT_YUV420P;
			break;
		case V4L2_PIX_FMT_YUYV:
			avfmt = AV_PIX_FMT_YUYV422;
			break;
		case V4L2_PIX_FMT_RGB565:
			avfmt = AV_PIX_FMT_RGB565LE;
			break;
		case V4L2_PIX_FMT_RGB24:
			avfmt = AV_PIX_FMT_RGB24;
			break;
		default:
			printf("!!! Unsupported v4l2 format: %d\n", v4lfmt);
			break;
	}

	return avfmt;
}

struct cvt_handle *convert_open(struct cvt_param param)
{
	struct cvt_handle *handle = malloc(sizeof(struct cvt_handle));
	if (!handle)
	{
		printf("--- malloc convert handle failed\n");
		return NULL;
	}

	CLEAR(*handle);
	handle->sws_ctx = NULL;
	handle->src_buffer = NULL;
	handle->src_buffersize = 0;
	handle->src_frame = NULL;
	handle->dst_buffer = NULL;
	handle->dst_buffersize = 0;
	handle->dst_frame = NULL;
	handle->inavfmt = AV_PIX_FMT_NONE;
	handle->outavfmt = AV_PIX_FMT_NONE;
	handle->params.inwidth = param.inwidth;
	handle->params.inheight = param.inheight;
	handle->params.inpixfmt = param.inpixfmt;
	handle->inavfmt = v4lFmt2AVFmt(handle->params.inpixfmt);
	handle->params.outwidth = param.outwidth;
	handle->params.outheight = param.outheight;
	handle->params.outpixfmt = param.outpixfmt;
	handle->outavfmt = v4lFmt2AVFmt(handle->params.outpixfmt);

	handle->sws_ctx = sws_getContext(handle->params.inwidth,
			handle->params.inheight, handle->inavfmt, handle->params.outwidth,
			handle->params.outheight, handle->outavfmt, SWS_BILINEAR, NULL,
			NULL, NULL);
	if (!handle->sws_ctx)
	{
		printf("--- Create scale context failed\n");
		goto err0;
	}

	// alloc buffers
	handle->src_frame = av_frame_alloc();
	if (!handle->src_frame)
	{
		printf("--- allocate src_frame failed\n");
		goto err1;
	}
	handle->src_buffersize = avpicture_get_size(handle->inavfmt,
			handle->params.inwidth, handle->params.inheight);
	handle->src_buffer = (uint8_t *) av_malloc(handle->src_buffersize);
	if (!handle->src_buffer)
	{
		printf("--- allocate src_buffer failed\n");
		goto err2;
	}
	avpicture_fill((AVPicture *) handle->src_frame, handle->src_buffer,
			handle->inavfmt, handle->params.inwidth, handle->params.inheight);

	handle->dst_frame = av_frame_alloc();
	if (!handle->dst_frame)
	{
		printf("--- allocate dst_frame failed\n");
		goto err3;
	}
	handle->dst_buffersize = avpicture_get_size(handle->outavfmt,
			handle->params.outwidth, handle->params.outheight);
	handle->dst_buffer = (uint8_t *) av_malloc(handle->dst_buffersize);
	if (!handle->dst_buffer)
	{
		printf("--- allocate dst_buffer failed\n");
		goto err4;
	}
	avpicture_fill((AVPicture *) handle->dst_frame, handle->dst_buffer,
			handle->outavfmt, handle->params.outwidth,
			handle->params.outheight);

	printf("+++ Convert Opened\n");
	return handle;

	err4: av_frame_free(&handle->dst_frame);
	err3: av_free(handle->src_buffer);
	err2: av_frame_free(&handle->src_frame);
	err1: sws_freeContext(handle->sws_ctx);
	err0: free(handle);
	return NULL;
}

void convert_close(struct cvt_handle *handle)
{
	av_free(handle->dst_buffer);
	av_frame_free(&handle->dst_frame);
	av_free(handle->src_buffer);
	av_frame_free(&handle->src_frame);
	sws_freeContext(handle->sws_ctx);
	free(handle);
	printf("+++ Convert Closed\n");
}

int convert_do(struct cvt_handle *handle, const void *inbuf, int isize,
		void **poutbuf, int *posize)
{
	assert(isize == handle->src_buffersize);
	memcpy(handle->src_buffer, inbuf, isize);
	sws_scale(handle->sws_ctx,
			(const uint8_t * const *) handle->src_frame->data,
			handle->src_frame->linesize, 0, handle->params.inheight,
			handle->dst_frame->data, handle->dst_frame->linesize);

	*poutbuf = handle->dst_buffer;
	*posize = handle->dst_buffersize;

	return 0;
}
