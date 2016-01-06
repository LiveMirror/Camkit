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

struct timestamp_param
{
	int startx;             // distance to the left (px)
	int starty;             // distance to the top (px)
	int width;              // the video width
	int factor;             // size of text, [0 .. 1]
};

struct timestamp_handle;

struct timestamp_handle *timestamp_open(struct timestamp_param params);

void timestamp_draw(struct timestamp_handle *handle, unsigned char *image);

void timestamp_close(struct timestamp_handle *handle);

#endif /* INCLUDE_TIMESTAMP_H_ */
