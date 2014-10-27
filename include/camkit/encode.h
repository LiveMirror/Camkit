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

#ifndef ENCODE_H
#define ENCODE_H
#include "comdef.h"

/**< h264 picture types */
enum pic_t
{
	NONE = -1, SPS, PPS, I, P, B
};

/**
 * encode parameters
 */
struct enc_param
{
		int src_picwidth; /**< the source picture width*/
		int src_picheight; /**< the source picture height */
		int enc_picwidth; /**< the encoded picture width */
		int enc_picheight; /**< the encode picture height */
		int fps; /**< frames per second */
		int bitrate; /**< bit rate (kbps), set bit rate to 0 means no rate control, the rate will depend on QP */
		int gop; /**< the size of group of pictures */
		int chroma_interleave; /**< whether chroma interleaved? */
};

/**< encode handle */
struct enc_handle;

/**
 * @brief Open a encode instance
 * @param param the encode parameters
 * @return the encode handle
 */
struct enc_handle *encode_open(struct enc_param param);
/**
 * @brief Close a encode intance
 * @param handle the encode handle
 */
void encode_close(struct enc_handle *handle);

/**
 * @brief Fetch H264 headers: SPS, PPS
 * 1. Repeatedly call the function till it returns 0
 * 2. Call the function before every encode_do()
 *
 * @param handle the encode handle
 * @param pbuf the point to point of the header buffer
 * @param plen the buffer size
 * @param type the frame type SPS/PPS
 * @return 1 if continue, 0 if reach end
 */
int encode_get_headers(struct enc_handle *handle, void **pbuf, int *plen,
		enum pic_t *type);

/**
 * @brief Encode a frame
 * @param handle the encode handle
 * @param buf the image buffer
 * @param len the buffer len
 * @param pbuf pointer to the buffer to save the frame
 * @param plen the buffer len pointer
 * @param type the frame type pointer
 * @return -1 on error, 0 on ok
 * @see encode_loop()
 */
int encode_do(struct enc_handle *handle, void *ibuf, int ilen, void **pobuf,
		int *polen, enum pic_t *type);

/**
 * @brief Set the quantization parameters
 * Note: the qp is used only when rate control is disable (bitrate is 0)
 *
 * @param handle the encode handle
 * @param val the new qp value, it's 0-51 for h264
 * @return 0 if ok, < 0 if error
 */
int encode_set_qp(struct enc_handle *handle, int val);

/**
 * @brief Set gop
 * @param handle the encode handle
 * @param val the new gop number
 * @return 0 if ok, < 0 if error
 */
int encode_set_gop(struct enc_handle *handle, int val);

/**
 * @brief Set bit rate
 * Note: the range is 0~32767
 * @param handle the encode handle
 * @param val the new bit rate (kbps)
 * @return 0 if ok, < 0 if error
 */
int encode_set_bitrate(struct enc_handle *handle, int val);

/**
 * @brief Set frame rate
 * Note: the frame rate should be greater than 0
 * @param handle the encode handle
 * @param val the new bit rate (kbps)
 * @return 0 if ok, < 0 if error
 */
int encode_set_framerate(struct enc_handle *handle, int val);

/**
 * @brief Force the next encoded frame to be I-frame
 * Note: It's better to call the function before encode_get_headers().
 *
 * @param handle the encode handle
 */
void encode_force_Ipic(struct enc_handle *handle);

#endif /* ENCODE_H */
