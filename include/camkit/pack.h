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

#ifndef INCLUDE_RTPPACK_H_
#define INCLUDE_RTPPACK_H_
#include "comdef.h"

struct pac_param
{
        int max_pkt_len;    // maximun packet length, better be less than MTU(1500)
        int framerate;
        int ssrc;
};

struct pac_handle;

struct pac_handle *pack_open(struct pac_param params);

void pack_put(struct pac_handle *handle, void *inbuf, int isize);

int pack_get(struct pac_handle *handle, void *outbuf, int bufsize,
        int *outsize);

void pack_close(struct pac_handle *handle);

#endif /* INCLUDE_RTPPACK_H_ */
