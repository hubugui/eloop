/*
 * event loop
 *
 * Copyright (c) 2023 hubugui <hubugui at gmail dot com> 
 *
 * This file is part of Eloop.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <pthread.h>

#include "event_loop_pool.h"

#define MAX_LOOP    1024

struct event_loop_pool {
    unsigned int number;
    struct event_loop *e_loops[MAX_LOOP];
    unsigned int pos;
    
    pthread_mutex_t mtx;
};

struct event_loop_pool *
event_loop_pool_create(unsigned int number)
{
    struct event_loop_pool *e_pool;

    if (number == 0)        number = 8;
    if (number > MAX_LOOP)  return NULL;

    e_pool = (struct event_loop_pool *) calloc(1, sizeof(*e_pool));
    if (e_pool) {
        if (pthread_mutex_init(&e_pool->mtx, NULL))
            goto ERROR;

        e_pool->number = number;
        e_pool->pos = 0;
        for (int i = 0; i < number; i++) {
            e_pool->e_loops[i] = event_loop_create();
            if (!e_pool->e_loops[i])
                goto ERROR;
        }
    }

    goto EXIT;
ERROR:
    event_loop_pool_delete(&e_pool);
    e_pool = NULL;
EXIT:
    return e_pool;
}

void 
event_loop_pool_delete(struct event_loop_pool **e_pool)
{
    if (e_pool && *e_pool) {
        for (int i = 0; i < (*e_pool)->number; i++) {
            if ((*e_pool)->e_loops[i])
                event_loop_delete((*e_pool)->e_loops[i]);
        }
        pthread_mutex_destroy(&(*e_pool)->mtx);

        free(*e_pool);
        *e_pool = NULL;
    }    
}

struct event_loop *
event_loop_pool_next(struct event_loop_pool *e_pool)
{
    struct event_loop *e_loop;

    pthread_mutex_lock(&e_pool->mtx);

    e_loop = e_pool->e_loops[e_pool->pos];
    if (++e_pool->pos >= e_pool->number)
        e_pool->pos = 0;

    pthread_mutex_unlock(&e_pool->mtx);
    return e_loop;
}

struct event_loop *
event_loop_pool_get_girst(struct event_loop_pool *e_pool)
{
    return e_pool->e_loops[0];
}
