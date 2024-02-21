/*
 * event io kqueue implementation
 *
 * Copyright (c) 2024 kyleliu <justfavme at gmail dot com> 
 * All rights reserved.
 *
 * This file is part of Eloop.
 */

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "event_io.h"
#include "event_channel.h"
#include "event_channel_map.h"

struct event_io {
    int kqfd;
    struct kevent events[FD_SETSIZE];
    struct kevent events_back[FD_SETSIZE];
};

struct event_io *
event_io_create(void)
{
    struct event_io *eio = (struct event_io *) calloc(1, sizeof(*eio));
    if (!eio) goto FAIL;

    eio->kqfd = kqueue();
    if (eio->kqfd == -1) goto FAIL;

    for (int i = 0; i < FD_SETSIZE; i++) {
        eio->events[i].ident = -1;
    }

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
    if (!eio) return;
    if (eio->kqfd != -1) close(eio->kqfd);
    free(eio);
    *eiop = NULL;
}

int 
event_io_add_fd(struct event_io *eio, struct event_channel *channel)
{
    int fd = event_channel_get_fd(channel);
    int mask = event_channel_get_mask(channel);
    int flag = 0;

    for (int i = 0; i < FD_SETSIZE; i++) {
        if (!(flag & FD_MASK_READ) && (mask & FD_MASK_READ) && (eio->events[i].ident == -1)) {
            EV_SET(&eio->events[i], fd, EVFILT_READ, EV_ADD, 0, 0, channel);
            flag |= FD_MASK_READ;
        }
        if (!(flag & FD_MASK_WRITE) && (mask & FD_MASK_WRITE) && (eio->events[i].ident == -1)) {
            EV_SET(&eio->events[i], fd, EVFILT_WRITE, EV_ADD, 0, 0, channel);
            flag |= FD_MASK_WRITE;
        }
        if (!(flag & FD_MASK_ERROR) && (mask & FD_MASK_ERROR) && (eio->events[i].ident == -1)) {
            EV_SET(&eio->events[i], fd, EVFILT_EXCEPT, EV_ADD, 0, 0, channel);
            flag |= FD_MASK_ERROR;
        }
        if (flag == mask) break;
    }

    if (kevent(eio->kqfd, eio->events, FD_SETSIZE, NULL, 0, NULL) == -1) {
        return -1;
    }
    return 0;
}

int 
event_io_remove_fd(struct event_io *eio, struct event_channel *channel)
{
    int fd = event_channel_get_fd(channel);
    int mask = event_channel_get_mask(channel);
    int flag = 0;

    for (int i = 0; i < FD_SETSIZE; i++) {
        struct kevent *event = NULL;
        if (!(flag & FD_MASK_READ) && (mask & FD_MASK_READ) && (eio->events[i].ident == fd)) {
            EV_SET(&eio->events[i], fd, EVFILT_READ, EV_DELETE, 0, 0, channel);
            event = &eio->events[i];
            flag |= FD_MASK_READ;
        }
        if (!(flag & FD_MASK_WRITE) && (mask & FD_MASK_WRITE) && (eio->events[i].ident == fd)) {
            EV_SET(&eio->events[i], fd, EVFILT_WRITE, EV_DELETE, 0, 0, channel);
            event = &eio->events[i];
            flag |= FD_MASK_WRITE;
        }
        if (!(flag & FD_MASK_ERROR) && (mask & FD_MASK_ERROR) && (eio->events[i].ident == fd)) {
            EV_SET(&eio->events[i], fd, EVFILT_EXCEPT, EV_DELETE, 0, 0, channel);
            event = &eio->events[i];
            flag |= FD_MASK_ERROR;
        }

        if (event != NULL) {
            if (kevent(eio->kqfd, event, 1, NULL, 0, NULL) == -1) {
                return -1;
            }
            event->ident = -1;
        }
        if (flag == mask) break;
    }
    return 0;
}

int 
event_io_poll(struct event_io *eio, struct event_channel_map *ec_map, unsigned long long timeout)
{
    int ret = 0;
    int n = 0;

    if (event_channel_map_get_length(ec_map) == 0)
        return 0;

    struct kevent events[FD_SETSIZE];
    struct timespec ts = {timeout / 1000, (timeout % 1000) * 1000 * 1000};
    n = kevent(eio->kqfd, NULL, 0, events, FD_SETSIZE, &ts);
    if (n < 0) {
        ret = -1;
        goto EXIT;
    }

    for (int i = 0; i < n; i++) {
        struct event_channel *channel = events[i].udata;
        int fd = event_channel_get_fd(channel);

        if (events[i].flags & EV_ERROR) {
            event_channel_on_error(channel);
            continue;
        }
        if (events[i].flags & EV_EOF) {
            event_channel_on_close(channel);
            continue;
        }
        if (events[i].filter == EVFILT_READ) {
            event_channel_on_read(channel);
            continue;
        }
        if (events[i].filter == EVFILT_WRITE) {
            event_channel_on_write(channel);
            continue;
        }
        if (events[i].filter == EVFILT_EXCEPT) {
            event_channel_on_error(channel);
            continue;
        }
    }

EXIT:
    return ret;
}