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
        int max_pkt_len;    // maximum packet length, better be less than MTU(1500)
        int ssrc;			// identifies the synchronization source, set the value randomly, with the intent that no two synchronization sources within the same RTP session will have the same SSRC
};

struct pac_handle;

struct pac_handle *pack_open(struct pac_param params);

/**
 * @brief Put one or more NALUs to be packed
 * @param handle the pack handle
 * @param inbuf the buffer pointed to one or more NALUs
 * @param isize the inbuf size
 */
void pack_put(struct pac_handle *handle, void *inbuf, int isize);

/**
 * @brief Get a requested packet
 * @param handle the pack handle
 * @param poutbuf the out packet
 * @param outsize the size of the packet
 * @note the bufsize should be bigger than *outsize
 */
int pack_get(struct pac_handle *handle, void **poutbuf, int *outsize);

void pack_close(struct pac_handle *handle);

#endif /* INCLUDE_RTPPACK_H_ */
