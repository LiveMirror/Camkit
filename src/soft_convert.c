/************************************************
 *  
 *  Camkit:soft_convert.c
 *
 *  Created on: Feb 11, 2015
 *  Author: Andy Huang
 *	Email: andyspider@126.com
 *
 *	All rights reserved!
 *	
 ************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <linux/videodev2.h>
#include "camkit/convert.h"

struct cvt_handle
{
	int src_buffersize;
	uint8_t *dst_buffer;
	int dst_buffersize;
	struct cvt_param params;
};

static void yuv422_to_yuv420(uint8_t *inbuf, uint8_t *outbuf, int width,
		int height)
{
	int i = 0, j = 0, k = 0;
	int UOffset = width * height;
	int VOffset = (width * height) * 5 / 4;
	int UVSize = (width * height) / 4;
	int line1 = 0, line2 = 0;
	int m = 0, n = 0;
	int y = 0, u = 0, v = 0;

	u = UOffset;
	v = VOffset;

	for (i = 0, j = 1; i < height; i += 2, j += 2)
	{
		/* Input Buffer Pointer Indexes */
		line1 = i * width * 2;
		line2 = j * width * 2;

		/* Output Buffer Pointer Indexes */
		m = width * y;
		y = y + 1;
		n = width * y;
		y = y + 1;

		/* Scan two lines at a time */
		for (k = 0; k < width * 2; k += 4)
		{
			unsigned char Y1, Y2, U, V;
			unsigned char Y3, Y4, U2, V2;

			/* Read Input Buffer */
			Y1 = inbuf[line1++];
			U = inbuf[line1++];
			Y2 = inbuf[line1++];
			V = inbuf[line1++];

			Y3 = inbuf[line2++];
			U2 = inbuf[line2++];
			Y4 = inbuf[line2++];
			V2 = inbuf[line2++];

			/* Write Output Buffer */
			outbuf[m++] = Y1;
			outbuf[m++] = Y2;

			outbuf[n++] = Y3;
			outbuf[n++] = Y4;

			outbuf[u++] = (U + U2) / 2;
			outbuf[v++] = (V + V2) / 2;
		}
	}
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
	handle->params.inwidth = param.inwidth;
	handle->params.inheight = param.inheight;
	handle->params.inpixfmt = param.inpixfmt;
	handle->params.outwidth = param.outwidth;
	handle->params.outheight = param.outheight;
	handle->params.outpixfmt = param.outpixfmt;

	if (handle->params.inpixfmt != V4L2_PIX_FMT_YUYV
			|| handle->params.outpixfmt != V4L2_PIX_FMT_YUV420)
	{
		printf("--- Only YUV422 to YUV420 converting is supported\n");
		goto err0;
	}

	if (handle->params.inwidth != handle->params.outwidth
			|| handle->params.inheight != handle->params.outheight)
	{
		printf("--- Scaling is not supported\n");
		goto err0;
	}

	handle->src_buffersize = handle->params.inwidth * handle->params.inheight
			* 16 / 8;		// YUV422 image size
	handle->dst_buffersize = handle->params.outwidth * handle->params.outheight
			* 12 / 8;		// YUV420 image size
	handle->dst_buffer = (uint8_t *) malloc(handle->dst_buffersize);
	if (!handle->dst_buffer)
	{
		printf("--- malloc dst_buffer failed\n");
		goto err0;
	}

	printf("+++ Convert Opened\n");
	return handle;

	err0: free(handle);
	return NULL;

}

void convert_close(struct cvt_handle *handle)
{
	free(handle->dst_buffer);
	free(handle);
	printf("+++ Convert Closed\n");
}

int convert_do(struct cvt_handle *handle, const void *inbuf, int isize,
		void **poutbuf, int *posize)
{
	if (isize != handle->src_buffersize)
	{
		printf("--- %s:%s in buffer size != src image size\n", __FILE__,
				__FUNCTION__);
		abort();
	}

	yuv422_to_yuv420((uint8_t *) inbuf, handle->dst_buffer,
			handle->params.inwidth, handle->params.inheight);

	*poutbuf = handle->dst_buffer;
	*posize = handle->dst_buffersize;

	return 0;
}

