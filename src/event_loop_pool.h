#ifndef __EVENT_LOOP_POOL_H__
#define __EVENT_LOOP_POOL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "event_loop.h"

struct event_loop_pool;

struct event_loop_pool *event_loop_pool_create(unsigned int thread_number);
void event_loop_pool_delete(struct event_loop_pool **e_pool);

struct event_loop *event_loop_pool_next(struct event_loop_pool *e_pool);
struct event_loop *event_loop_pool_get_girst(struct event_loop_pool *e_pool);

#ifdef __cplusplus
}
#endif
#endif
