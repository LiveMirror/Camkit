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

#ifndef CONVERT_H
#define CONVERT_H
#include "comdef.h"

/**
 * convert parameters
 */
struct cvt_param
{
		int inwidth; /**< input image width */
		int inheight; /**< input image height */
		U32 inpixfmt; /**< input image pixel format */
		int outwidth; /**< output image width */
		int outheight; /**< output image height */
		U32 outpixfmt; /**< output image pixel format */
};

/**< convert handle */
struct cvt_handle;

/**
 * @brief Open a convert instance
 * @param param the convert parameters
 * @return the convert handle
 */
struct cvt_handle *convert_open(struct cvt_param param);
/**
 * @brief Close a convert instance
 * @param param the convert handle
 */
void convert_close(struct cvt_handle *handle);

/**
 * @brief Convert an image
 * @param handle the convert handle
 * @param inbuf the input image buffer
 * @param ilen the input image size
 * @param poutbuf the point to the output image buffer
 * @param polen the output buffer size pointer
 * @return ok: 0, error: < 0
 */
int convert_do(struct cvt_handle *handle, const void *inbuf, int isize,
		void **poutbuf, int *posize);

#endif /* CONVERT_H */
