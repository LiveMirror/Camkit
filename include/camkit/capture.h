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

#ifndef CAPTURE_H
#define CAPTURE_H
#include "comdef.h"

/**
 * capture parameters
 */
struct cap_param
{
		char *dev_name; /**< video device location, eg: /dev/video0 */
		int width; /**< video width */
		int height; /**< video height */
		U32 pixfmt; /**< video pixel format */
		int rate; /**< video rate */
};

/**< capture handle */
struct cap_handle;

/**
 * @brief Open a capture device
 * @param param the capture parameters
 * @return the capture handle
 */
struct cap_handle *capture_open(struct cap_param param);
/**
 * @brief Close a capture device
 * @param handle the capture handle to be closed
 */
void capture_close(struct cap_handle *handle);

/**
 * @brief Start a capture stream
 * @param handle the capture handle
 */
int capture_start(struct cap_handle *handle);

/**
 * @brief Stop a capture stream
 * @param handle the capture handle
 */
void capture_stop(struct cap_handle *handle);

/**
 * @brief Fetch captured image on every step
 * Note: this is non-consistent with capture_loop()
 *
 * @param handle the capture handle
 * @param pbuf the point to the output image buffer
 * @param plen the point to the length buffer
 * @return -1 if timeout or error, 0 if ok, > 0 if non fatal error
 */
int capture_get_data(struct cap_handle *handle, void **pbuf, int *plen);

int capture_query_brightness(struct cap_handle *handle, int *min, int *max, int *step);
int capture_get_brightness(struct cap_handle *handle, int *val);
int capture_set_brightness(struct cap_handle *handle, int val);

int capture_query_contrast(struct cap_handle *handle, int *min, int *max, int *step);
int capture_get_contrast(struct cap_handle *handle, int *val);
int capture_set_contrast(struct cap_handle *handle, int val);

int capture_query_saturation(struct cap_handle *handle, int *min, int *max, int *step);
int capture_get_saturation(struct cap_handle *handle, int *val);
int capture_set_saturation(struct cap_handle *handle, int val);

#endif /* CAPTURE_H */
