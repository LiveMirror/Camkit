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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "camkit/network.h"

struct net_handle
{
    int sktfd;
    struct sockaddr_in server_sock;
    int sersock_len;

    struct net_param params;
};

struct net_handle *net_open(struct net_param params)
{
    struct net_handle *handle = (struct net_handle *) malloc(
            sizeof(struct net_handle));
    if (!handle)
    {
        printf("--- malloc net handle failed\n");
        return NULL;
    }

    CLEAR(*handle);
    handle->params.type = params.type;
    handle->params.serip = params.serip;
    handle->params.serport = params.serport;

    if (handle->params.type == TCP)
        handle->sktfd = socket(AF_INET, SOCK_STREAM, 0);
    else
        // UDP
        handle->sktfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (handle->sktfd < 0)
    {
        printf("--- create socket failed\n");
        free(handle);
        return NULL;
    }

    handle->server_sock.sin_family = AF_INET;
    handle->server_sock.sin_port = htons(handle->params.serport);
    handle->server_sock.sin_addr.s_addr = inet_addr(handle->params.serip);
    handle->sersock_len = sizeof(handle->server_sock);

    int ret = connect(handle->sktfd, (struct sockaddr *) &handle->server_sock,
            handle->sersock_len);
    if (ret < 0)
    {
        printf("--- connect to server failed\n");
        close(handle->sktfd);
        free(handle);
        return NULL;
    }

    printf("+++ Network Opened\n");
    return handle;
}

void net_close(struct net_handle *handle)
{
    close(handle->sktfd);
    free(handle);
    printf("+++ Network Closed\n");
}

int net_send(struct net_handle *handle, void *data, int size)
{
    return send(handle->sktfd, data, size, 0);
}

int net_recv(struct net_handle *handle, void *data, int size)
{
    return recv(handle->sktfd, data, size, 0);
}
