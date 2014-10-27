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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <linux/ipu.h>
#include "camkit/convert.h"

struct cvt_handle
{
	int fd;
	struct ipu_task task;
	void *ipu_inbuf;
	void *ipu_outbuf;
	int ipu_insize;
	int ipu_outsize;
	struct cvt_param params;
	int quit;
};

// Note: IPU_PIX* is the same as V4L2_PIX*, so we can use either in the function
static unsigned int fmt2bpp(unsigned int pixelformat)
{
	unsigned int bpp;

	switch (pixelformat)
	{
		case IPU_PIX_FMT_RGB565:
			/*interleaved 422*/
		case IPU_PIX_FMT_YUYV:
		case IPU_PIX_FMT_UYVY:
			/*non-interleaved 422*/
		case IPU_PIX_FMT_YUV422P:
		case IPU_PIX_FMT_YVU422P:
			bpp = 16;
			break;
		case IPU_PIX_FMT_BGR24:
		case IPU_PIX_FMT_RGB24:
		case IPU_PIX_FMT_YUV444:
		case IPU_PIX_FMT_YUV444P:
			bpp = 24;
			break;
		case IPU_PIX_FMT_BGR32:
		case IPU_PIX_FMT_BGRA32:
		case IPU_PIX_FMT_RGB32:
		case IPU_PIX_FMT_RGBA32:
		case IPU_PIX_FMT_ABGR32:
			bpp = 32;
			break;
			/*non-interleaved 420*/
		case IPU_PIX_FMT_YUV420P:
		case IPU_PIX_FMT_YVU420P:
		case IPU_PIX_FMT_YUV420P2:
		case IPU_PIX_FMT_NV12:
		case IPU_PIX_FMT_TILED_NV12:
			bpp = 12;
			break;
		default:
			bpp = 8;
			break;
	}
	return bpp;
}

struct cvt_handle *convert_open(struct cvt_param param)
{
	struct cvt_handle *handle = malloc(sizeof(struct cvt_handle));
	if (!handle)
	{
		printf("--- malloc convert handle failed\n");
		return NULL ;
	}

	CLEAR(*handle);
	handle->quit = 0;
	handle->params.inwidth = param.inwidth;
	handle->params.inheight = param.inheight;
	handle->params.inpixfmt = param.inpixfmt;
	handle->params.outwidth = param.outwidth;
	handle->params.outheight = param.outheight;
	handle->params.outpixfmt = param.outpixfmt;

	int ret;

	handle->fd = open("/dev/mxc_ipu", O_RDWR, 0);
	if (handle->fd < 0)
	{
		printf("--- Open ipu device failed\n");
		ret = -1;
		goto err;
	}

	memset(&handle->task, 0, sizeof(handle->task));
	handle->task.input.width = handle->params.inwidth;
	handle->task.input.height = handle->params.inheight;
	handle->task.input.format = handle->params.inpixfmt;
	handle->task.output.width = handle->params.outwidth;
	handle->task.output.height = handle->params.outheight;
	handle->task.output.format = handle->params.outpixfmt;

	handle->ipu_insize = handle->task.input.paddr = handle->task.input.width
			* handle->task.input.height * fmt2bpp(handle->task.input.format)
			/ 8;

	ret = ioctl(handle->fd, IPU_ALLOC, &handle->task.input.paddr);
	if (ret < 0)
	{
		printf("--- ipu inbuf ioctl IPU_ALLOC failed: %s\n", strerror(errno));
		goto err;
	}

	handle->ipu_inbuf = mmap(NULL, handle->ipu_insize, PROT_READ | PROT_WRITE,
			MAP_SHARED, handle->fd, handle->task.input.paddr);
	if (!handle->ipu_inbuf)
	{
		printf("--- mmap handle->ipu_inbuf failed\n");
		goto err;
	}

	// out
	handle->ipu_outsize = handle->task.output.paddr = handle->task.output.width
			* handle->task.output.height * fmt2bpp(handle->task.output.format)
			/ 8;
	ret = ioctl(handle->fd, IPU_ALLOC, &handle->task.output.paddr);
	if (ret < 0)
	{
		printf("--- ipu outbuf ioctl IPU_ALLOC failed\n");
		goto err;
	}

	handle->ipu_outbuf = mmap(NULL, handle->ipu_outsize, PROT_READ | PROT_WRITE,
			MAP_PRIVATE, handle->fd, handle->task.output.paddr);
	if (ret < 0)
	{
		printf("--- mmap ipu_outbuf failed\n");
		goto err;
	}

	printf("+++ Convert Opened\n");
	return handle;

	err: if (handle->ipu_inbuf)
	{
		munmap(handle->ipu_inbuf, handle->ipu_insize);
		handle->ipu_inbuf = NULL;
	}
	if (handle->ipu_outbuf)
	{
		munmap(handle->ipu_outbuf, handle->ipu_outsize);
		handle->ipu_outbuf = NULL;
	}
	if (handle->task.output.paddr)
	{
		ioctl(handle->fd, IPU_FREE, &handle->task.output.paddr);
		handle->task.output.paddr = 0;
	}
	if (handle->task.input.paddr)
	{
		ioctl(handle->fd, IPU_FREE, &handle->task.input.paddr);
		handle->task.input.paddr = 0;
	}
	if (handle->fd)
	{
		close(handle->fd);
		handle->fd = -1;
	}
	free(handle);

	return NULL ;
}

void convert_close(struct cvt_handle *handle)
{
	if (handle->ipu_inbuf)
	{
		munmap(handle->ipu_inbuf, handle->ipu_insize);
		handle->ipu_inbuf = NULL;
	}
	if (handle->ipu_outbuf)
	{
		munmap(handle->ipu_outbuf, handle->ipu_outsize);
		handle->ipu_outbuf = NULL;
	}
	if (handle->task.output.paddr)
	{
		ioctl(handle->fd, IPU_FREE, &handle->task.output.paddr);
		handle->task.output.paddr = 0;
	}
	if (handle->task.input.paddr)
	{
		ioctl(handle->fd, IPU_FREE, &handle->task.input.paddr);
		handle->task.input.paddr = 0;
	}
	if (handle->fd)
	{
		close(handle->fd);
		handle->fd = -1;
	}

	free(handle);
	handle = NULL;
	printf("+++ Convert Closed\n");
}

int convert_do(struct cvt_handle *handle, const void *ibuf, int ilen,
		void **obuf, int *olen)
{
	if (ilen != handle->ipu_insize)
	{
		printf("--- %s:%s in buffer size != ipu insize\n", __FILE__, __FUNCTION__);
		abort();
	}

	memcpy(handle->ipu_inbuf, ibuf, handle->ipu_insize);

	int ret = ioctl(handle->fd, IPU_QUEUE_TASK, &handle->task);
	if (ret < 0)
	{
		printf("--- ipu convert failed\n");
		return -1;
	}

	*obuf = handle->ipu_outbuf;
	*olen = handle->ipu_outsize;

	return 0;
}
