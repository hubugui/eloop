/*
 * event loop
 *
 * Copyright (c) 2023 hubugui at gmail.com
 *
 * This file is part of Eloop.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <pthread.h>

#include "event_io.h"
#include "event_loop.h"
#include "event_channel_map.h"
#include "common/list.h"

struct event_timer {
    long long id;
    unsigned int interval_ms;
    enum timer_type type;
    event_loop_timer_proc on_timer;
    void *userdata;
    struct timespec ts;
};

struct event_job {
    event_loop_job_proc on_job;
    void *userdata1;
    void *userdata2;
    void *userdata3;
};

struct event_loop {
    /* timer */
    long long tid;

    struct list *timer_list;
    pthread_mutex_t timer_mtx;

    /* job */
    struct list *job_list;
    pthread_mutex_t job_mtx;

    /* fd */
    pthread_mutex_t fd_mtx;
    struct event_io *fd_io;
    struct event_channel_map *ec_map;
    unsigned int fd_amount;
    int max_fd;

    /* milliseconds */
    unsigned int interval_ms;

    /* thread */
    int is_thread_ready;
    pthread_t thread_fd;
    int thread_abort;

    pthread_mutex_t proc_mtx;
    pthread_cond_t proc_cond;
};

static void 
_wakeup_thread(struct event_loop *eloop)
{
    pthread_mutex_lock(&eloop->proc_mtx);
    pthread_cond_signal(&eloop->proc_cond);
    pthread_mutex_unlock(&eloop->proc_mtx);
}

static void _job_free(struct event_job *job)
{
    if (job)  free(job);
}

static int 
_job_add(struct event_loop *eloop, struct event_job *job)
{
    struct event_job *_job = (struct event_job *) calloc(1, sizeof(*_job));

    if (_job) {
        memmove(_job, job, sizeof(*_job));

        pthread_mutex_lock(&eloop->job_mtx);
        list_append(eloop->job_list, _job);
        pthread_mutex_unlock(&eloop->job_mtx);
    }

    return job ? 0 : -1;
}

static int 
_job_proc(struct event_loop *eloop, int is_remove_all)
{
    int ret = 0;

    pthread_mutex_lock(&eloop->job_mtx);

    if (is_remove_all) {
        list_remove_all(eloop->job_list, _job_free);
        goto EXIT;
    }

    for (struct list_node *node = list_get_head(eloop->job_list); node != NULL; /**/) {
        struct list_node *next = list_get_next(node);
        struct event_job *job = (struct event_job *) list_get_data(node);

        /* notify when fire */
        job->on_job(eloop, job->userdata1, job->userdata2, job->userdata3);
        /* remove */
        list_remove_node(eloop->job_list, node, _job_free);

        node = next;
    }

EXIT:
    pthread_mutex_unlock(&eloop->job_mtx);
    return ret;
}

static int 
_fd_proc(struct event_loop *eloop, unsigned long long timeout)
{
    int ret = 0;

    pthread_mutex_lock(&eloop->fd_mtx);
    ret = event_io_poll(eloop->fd_io, eloop->ec_map, timeout);
    pthread_mutex_unlock(&eloop->fd_mtx);

    return ret;
}

static void 
_ts_plus(struct timespec *ts, unsigned int interval_ms)
{
    static const long one_seconds = 1000 * 1000 * 1000;

    ts->tv_sec += interval_ms / 1000;
    ts->tv_nsec += (interval_ms % 1000) * 1000 * 1000;
    if (ts->tv_nsec >= one_seconds) {
        ts->tv_sec += ts->tv_nsec / one_seconds;
        ts->tv_nsec %= one_seconds;
    }
}

static int 
_timer_compare(struct timespec      *ts1, struct timespec *ts2)
{
    int ret = 0;
    long long t1 = ts1->tv_sec * 1000 * 1000 * 1000 + ts1->tv_nsec;
    long long t2 = ts2->tv_sec * 1000 * 1000 * 1000 + ts2->tv_nsec;

    if (t1 < t2)        ret = -1;
    else if (t1 > t2)   ret = 1;
    return ret;
}

static long long 
_timer_add(struct event_loop *eloop, struct event_timer *tm)
{
    long long ret = 0;
    struct event_timer *timer = (struct event_timer *) calloc(1, sizeof(*timer));

    if (timer) {
        struct list_node *node = NULL;
        char inserted = 0;

        *timer = *tm;

        pthread_mutex_lock(&eloop->timer_mtx);

        /* id */
        if (tm->id < 1) {
            if (++eloop->tid <= 0)
                eloop->tid = 1;
            ret = timer->id = eloop->tid;
        } else
            ret = tm->id;

        /* increment */
        for (node = list_get_head(eloop->timer_list); node != NULL; node = list_get_next(node)) {
            struct event_timer *timer_item = (struct event_timer *) list_get_data(node);
            if (_timer_compare(&timer->ts, &timer_item->ts) <= 0) {
                list_append_before(eloop->timer_list, timer, node);
                inserted = 1;
                break;
            }
        }

        /* tail */
        if (inserted == 0)  list_append(eloop->timer_list, timer);

        pthread_mutex_unlock(&eloop->timer_mtx);
    } else
        ret = -1;

    return ret;
}

static void _timer_free(struct event_timer *timer)
{
    if (timer)  free(timer);
}

static int 
_timer_remove(struct event_loop *eloop, long long id)
{
    int ret = -1;
    struct list_node *node = NULL;

    pthread_mutex_lock(&eloop->timer_mtx);

    for (node = list_get_head(eloop->timer_list); node != NULL; node = list_get_next(node)) {
        struct event_timer *timer_item = (struct event_timer *) list_get_data(node);

        if (timer_item->id == id) {
            list_remove_node(eloop->timer_list, node, _timer_free);
            ret = 0;
            break;
        }
    }

    pthread_mutex_unlock(&eloop->timer_mtx);
    return ret;
}

static unsigned int 
_timer_min(struct event_loop *eloop)
{
    unsigned int ret = 0xffffffff;

    pthread_mutex_lock(&eloop->timer_mtx);

    if (list_length(eloop->timer_list) > 0) {
        struct list_node *node = list_get_head(eloop->timer_list);
        struct event_timer *timer_head = (struct event_timer *) list_get_data(node);
        ret = timer_head->interval_ms;
    }

    pthread_mutex_unlock(&eloop->timer_mtx);
    return ret;
}

static void 
_timer_proc(struct event_loop *eloop, int is_remove_all)
{
    struct timespec now = {0};

    pthread_mutex_lock(&eloop->timer_mtx);

    if (is_remove_all) {
        list_remove_all(eloop->timer_list, _timer_free);
        goto EXIT;
    }

    for (struct list_node *node = list_get_head(eloop->timer_list); node != NULL; /**/) {
        struct list_node *next = list_get_next(node);
        struct event_timer *timer = (struct event_timer *) list_get_data(node);

        clock_gettime(CLOCK_MONOTONIC, &now);
        if (_timer_compare(&now, &timer->ts) >= 0) {
            /* notify when fire */
            timer->on_timer(eloop, timer->id, timer->userdata);
            /* remove */
            list_remove_node(eloop->timer_list, node, NULL);
            /* next */
            node = next;

            if (timer->type == timer_type_one_shot) {
                free(timer);
                continue;
            } else {
                timer->ts = now;
                _ts_plus(&timer->ts, timer->interval_ms);
                _timer_add(eloop, timer);
                free(timer);
            }
        } else
            break;
    }

EXIT:
    pthread_mutex_unlock(&eloop->timer_mtx);
}

static void *
_thread_func(void *userdata)
{
    struct event_loop *eloop = (struct event_loop *) userdata;
    unsigned long long interval, timer_min_interval, fd_min_interval = 10;

    while (!eloop->thread_abort) {
        timer_min_interval = _timer_min(eloop);
        interval = timer_min_interval <= fd_min_interval ? timer_min_interval : fd_min_interval;

        _fd_proc(eloop, interval);
        _timer_proc(eloop, 0);
        _job_proc(eloop, 0);
    }

    _timer_proc(eloop, 1);
    _job_proc(eloop, 1);
    return (void *) 0;
}

struct event_loop *
event_loop_create(void)
{
    pthread_mutexattr_t mtx_attr;
    pthread_condattr_t cond_attr;
    unsigned int max_fd;
    int ret;
    struct event_loop *eloop = (struct event_loop *) calloc(1, sizeof(*eloop));

    if (!eloop)
        goto FAIL;

    /* mtx_attr */
    if (pthread_mutexattr_init(&mtx_attr)) {
        free(eloop);
        return NULL;
    }    
    if (pthread_mutexattr_settype(&mtx_attr, PTHREAD_MUTEX_RECURSIVE)) {
        pthread_mutexattr_destroy(&mtx_attr);
        free(eloop);
        return NULL;
    }

    /* cond_attr */
    ret = pthread_condattr_init(&cond_attr);
    if (ret) {
        pthread_mutexattr_destroy(&mtx_attr);
        free(eloop);
        return NULL;
    }
    /* pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC); */

    /* timer */
    if (pthread_mutex_init(&eloop->timer_mtx, &mtx_attr))
        goto FAIL;
    eloop->timer_list = list_create();

    /* job */
    if (pthread_mutex_init(&eloop->job_mtx, &mtx_attr))
        goto FAIL;
    eloop->job_list = list_create();

    /* fd */
    if (pthread_mutex_init(&eloop->fd_mtx, &mtx_attr))
        goto FAIL;
    eloop->fd_amount = 0;
    eloop->fd_io = event_io_create();
    if (!eloop->fd_io)
        goto FAIL;
    eloop->ec_map = event_channel_map_create();
    if (!eloop->ec_map)
        goto FAIL;    

    /* thread */
    if (pthread_mutex_init(&eloop->proc_mtx, &mtx_attr))
        goto FAIL;    
    if (pthread_cond_init(&eloop->proc_cond, &cond_attr))
        goto FAIL;
    eloop->interval_ms = 10;
    eloop->thread_abort = 0;

    ret = pthread_create(&eloop->thread_fd, NULL, _thread_func, (void *) eloop);
    if (ret == 0)
        eloop->is_thread_ready = 1;
    else
        goto FAIL;

    goto EXIT;
FAIL:
    pthread_condattr_destroy(&cond_attr);
    pthread_mutexattr_destroy(&mtx_attr);
    event_loop_delete(&eloop);
EXIT:
    return eloop;
}

void 
event_loop_delete(struct event_loop **eloop)
{
    if (eloop && *eloop) {
        struct event_loop *ep = *eloop;

        /* thread */
        if (ep->is_thread_ready) {
            ep->thread_abort = 1;
            pthread_join(ep->thread_fd, NULL);
        }
        pthread_cond_destroy(&ep->proc_cond);
        pthread_mutex_destroy(&ep->proc_mtx);

        /* timer */
        list_delete(&ep->timer_list, _timer_free);
        pthread_mutex_destroy(&ep->timer_mtx);

        /* job */
        list_delete(&ep->job_list, _job_free);
        pthread_mutex_destroy(&ep->job_mtx);

        /* fd */
        event_io_delete(&ep->fd_io);
        event_channel_map_delete(&ep->ec_map);
        pthread_mutex_destroy(&ep->fd_mtx);

        free(ep);
        *eloop = NULL;
    }
}

long long 
event_loop_add_timer(struct event_loop *eloop, 
                           unsigned int interval_ms,
                           enum timer_type type,
                           event_loop_timer_proc on_timer,
                           void *userdata)
{
    long long id = 0;
    struct event_timer timer = {0};

    if (!on_timer)  return -1;

    timer.id = -1;
    timer.interval_ms = interval_ms;
    timer.type = type;
    timer.on_timer = on_timer;
    timer.userdata = userdata;    

    clock_gettime(CLOCK_MONOTONIC, &timer.ts);
    _ts_plus(&timer.ts, timer.interval_ms);
    if ((id = _timer_add(eloop, &timer)) > 0)
        _wakeup_thread(eloop);
    return id;
}

int 
event_loop_remove_timer(struct event_loop *eloop, long long id)
{
    return _timer_remove(eloop, id);
}

int 
event_loop_add_job(struct event_loop *eloop, 
                           event_loop_job_proc on_job, 
                           void *userdata1,
                           void *userdata2,
                           void *userdata3)
{
    int ret = 0;
    struct event_job job = {0};

    if (!on_job)  return -1;

    job.on_job = on_job;
    job.userdata1 = userdata1;
    job.userdata2 = userdata2;
    job.userdata3 = userdata3;
    return _job_add(eloop, &job);
}

int 
event_loop_add_channel(struct event_loop *eloop, struct event_channel *channel)
{
    int ret = 0;

    pthread_mutex_lock(&eloop->fd_mtx);

    ret = event_channel_map_add(eloop->ec_map, channel);
    if (ret == 0)   event_io_add_fd(eloop->fd_io, channel);

    pthread_mutex_unlock(&eloop->fd_mtx);
    return ret;
}

int 
event_loop_remove_fd(struct event_loop *eloop, int fd, int mask, int *is_delete)
{
    struct event_channel *channel = NULL;

    *is_delete = 0;

    pthread_mutex_lock(&eloop->fd_mtx);

    channel = event_channel_map_find(eloop->ec_map, fd);
    if (channel) {
        /* origin */
        int origin_mask = event_channel_get_mask(channel);

        /* update to remove mask */
        event_channel_set_mask(channel, mask);
        /* remove mark with mask */
        event_io_remove_fd(eloop->fd_io, channel);
        /* reset to origin */
        event_channel_set_mask(channel, origin_mask);
        event_channel_remove_mask(channel, mask);

        /* remove when NONE */
        if (event_channel_get_mask(channel) == FD_MASK_NONE) {
            event_channel_map_remove(eloop->ec_map, fd);
            *is_delete = 1;
        }
    }

    pthread_mutex_unlock(&eloop->fd_mtx);
    return channel ? 0 : -1;
}

int 
event_loop_remove_channel(struct event_loop *eloop, struct event_channel *channel)
{
    int fd = event_channel_get_fd(channel);

    pthread_mutex_lock(&eloop->fd_mtx);

    /* remove mark with mask */
    event_io_remove_fd(eloop->fd_io, channel);

    /* delete when NONE */
    event_channel_map_remove(eloop->ec_map, fd);

    pthread_mutex_unlock(&eloop->fd_mtx);
    return 0;
}

int 
event_loop_update_channel(struct event_loop *eloop, struct event_channel *channel)
{
    int ret = 0;
    int origin_mask = event_channel_get_mask(channel);

    pthread_mutex_lock(&eloop->fd_mtx);

    event_channel_set_mask(channel, FD_MASK_READ | FD_MASK_WRITE | FD_MASK_ERROR);
    event_io_remove_fd(eloop->fd_io, channel);

    event_channel_set_mask(channel, origin_mask);
    ret = event_io_add_fd(eloop->fd_io, channel);

    pthread_mutex_unlock(&eloop->fd_mtx);
    return ret;
}
