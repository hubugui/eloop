/*
 * sleep
 *
 * Copyright (c) 2023 hubugui <hubugui at gmail dot com> 
 * All rights reserved.
 *
 * This file is part of Eloop.
 */

#include <errno.h>
#include <time.h>

#include "sleep.h"

void 
sleep_ms(unsigned int ms)
{
    struct timespec req, rem;

    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000;

    while (nanosleep(&req, &rem) == -1 && errno == EINTR) {
        req = rem;
    }
}
