/*
 * event select
 *
 * Copyright (c) 2023 hubugui <hubugui at gmail dot com> 
 * All rights reserved.
 *
 * This file is part of Eloop.
 */

#include <string.h>
#include <stdlib.h>

#if defined(__linux) || defined(__linux__) 
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#elif defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/event.h>
#include <sys/time.h>
#elif defined(WIN32) || defined(_WIN32) 
#include <WinSock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#endif

#include <stdio.h>

#include "event_io.h"
#include "event_channel.h"
#include "event_channel_map.h"
#include "common/list.h"

struct event_io {
    fd_set fds_read;
    fd_set fds_write;
    fd_set fds_exp;

    fd_set fds_read_back;
    fd_set fds_write_back;
    fd_set fds_exp_back;

    int max_fd;
    char fds_is_dirty;
};

struct event_io *
event_io_create(void)
{
    struct event_io *eio = (struct event_io *) calloc(1, sizeof(*eio));

    if (eio == NULL)            goto FAIL;

    FD_ZERO(&eio->fds_read);
    FD_ZERO(&eio->fds_write);
    FD_ZERO(&eio->fds_exp);

    FD_ZERO(&eio->fds_read_back);
    FD_ZERO(&eio->fds_write_back);
    FD_ZERO(&eio->fds_exp_back);

    eio->max_fd = -1;
    eio->fds_is_dirty = 1;

    goto EXIT;
FAIL:
    event_io_delete(&eio);
EXIT:
    return eio;
}

void 
event_io_delete(struct event_io **eiop)
{
    struct event_io *eio = eiop && (*eiop) ? (*eiop) : NULL;
    if (!eio)   return;

    free(eio);
    *eiop = NULL;
}

int 
event_io_add_fd(struct event_io *eio, struct event_channel *channel)
{
    if (event_channel_is_exist_mask(channel, FD_MASK_READ))    FD_SET(event_channel_get_fd(channel), &eio->fds_read);
    if (event_channel_is_exist_mask(channel, FD_MASK_WRITE))   FD_SET(event_channel_get_fd(channel), &eio->fds_write);
    if (event_channel_is_exist_mask(channel, FD_MASK_ERROR))   FD_SET(event_channel_get_fd(channel), &eio->fds_exp);

    eio->fds_is_dirty = 1;
    return 0;
}

int 
event_io_remove_fd(struct event_io *eio, struct event_channel *channel)
{
    if (event_channel_is_exist_mask(channel, FD_MASK_READ))    FD_CLR(event_channel_get_fd(channel), &eio->fds_read);
    if (event_channel_is_exist_mask(channel, FD_MASK_WRITE))   FD_CLR(event_channel_get_fd(channel), &eio->fds_write);
    if (event_channel_is_exist_mask(channel, FD_MASK_ERROR))   FD_CLR(event_channel_get_fd(channel), &eio->fds_exp);

    eio->fds_is_dirty = 1;
    return 0;
}

int 
event_io_poll(struct event_io *eio, struct event_channel_map *ec_map, unsigned long long timeout)
{
    int ret = 0;
    int live_count = 0;
    struct timeval tv = {timeout / 1000, (timeout % 1000) * 1000};

    if (event_channel_map_get_length(ec_map) == 0)
        return 0;

    FD_ZERO(&eio->fds_read_back);
    memcpy(&eio->fds_read_back, &eio->fds_read, sizeof(fd_set));

    FD_ZERO(&eio->fds_write_back);
    memcpy(&eio->fds_write_back, &eio->fds_write, sizeof(fd_set));

    FD_ZERO(&eio->fds_exp_back);
    memcpy(&eio->fds_exp_back, &eio->fds_exp, sizeof(fd_set));

    /* 1. find max fd and use back fd to select */
    if (eio->fds_is_dirty) {
        eio->fds_is_dirty = 0;
        eio->max_fd = event_channel_map_get_max_fd(ec_map);
#if 0
        printf("%s>%d>max_fd=%d\n", __FUNCTION__, __LINE__, eio->max_fd);
#endif
    }

    /* 2. select */
    ret = select(eio->max_fd + 1, &eio->fds_read_back, &eio->fds_write_back, &eio->fds_exp_back, &tv);
    if (ret < 0) {
#if defined(__linux) || defined(__linux__)
        if(errno == EINTR)
            return 0;
#elif defined(WIN32) || defined(_WIN32) 
        ret = GetLastError();
#endif

#if 0
        printf("%s>%d>max_fd=%d, ret=%d\n", __FUNCTION__, __LINE__, eio->max_fd, ret);
#endif
        return ret;
    }

    /* 3. fill */
    if (ret > 0) {
        int event_ret = 0;
        void *meta = NULL;

        for (struct event_channel *channel = event_channel_map_get_head(ec_map, &meta)
            ; channel != NULL && meta != NULL
            ; /**/) {
            int fd = event_channel_get_fd(channel);
            struct event_channel *channel_next = event_channel_map_get_next(ec_map, &meta);

#if 0
            printf("%s>%d>fd=%d, ret=%d, r=%d, w=%d, e=%d\n", __FUNCTION__, __LINE__, fd, ret
                    , FD_ISSET(fd, &eio->fds_read_back)
                    , FD_ISSET(fd, &eio->fds_write_back)
                    , FD_ISSET(fd, &eio->fds_exp_back));
#endif
            if (FD_ISSET(fd, &eio->fds_read_back))     event_ret = event_channel_on_read(channel);
            if (FD_ISSET(fd, &eio->fds_write_back))    event_ret = event_channel_on_write(channel);
            if (FD_ISSET(fd, &eio->fds_exp_back))      event_ret = event_channel_on_error(channel);

            live_count++;

            channel = channel_next;
        }
    }

    return live_count;
}
