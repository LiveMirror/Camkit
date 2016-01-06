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

#ifndef INCLUDE_TIMESTAMP_H_
#define INCLUDE_TIMESTAMP_H_
#include "comdef.h"

struct tms_param
{
	int startx;             // distance to the left (px)
	int starty;             // distance to the top (px)
	int video_width;        // the video width
	int factor;             // size of text, [0 or 1]
};

struct tms_handle;

struct tms_handle *timestamp_open(struct tms_param params);

void timestamp_draw(struct tms_handle *handle, unsigned char *image);

void timestamp_close(struct tms_handle *handle);

#endif /* INCLUDE_TIMESTAMP_H_ */
