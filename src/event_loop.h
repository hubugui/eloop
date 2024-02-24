#ifndef __EVENT_LOOP_H__
#define __EVENT_LOOP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "event.h"
#include "event_channel.h"

struct event_loop;

enum timer_type {
    timer_type_one_shot,
    timer_type_forever,    
};

typedef int (*event_loop_fd_proc)(struct event_loop *eloop, 
                               int fd, 
                               enum fd_mask mask,
                               void *userdata);
typedef int (*event_loop_timer_proc)(struct event_loop *eloop, 
                                  long long id, 
                                  void *userdata);
typedef int (*event_loop_job_proc)(struct event_loop *eloop, 
                                void *userdata1,
                                void *userdata2,
                                void *userdata3);

struct event_loop *event_loop_create(void);
void event_loop_delete(struct event_loop **eloop);

int event_loop_add_channel(struct event_loop *eloop, struct event_channel *channel);
int event_loop_remove_fd(struct event_loop *eloop, int fd, int mask, int *is_delete);
int event_loop_remove_channel(struct event_loop *eloop, struct event_channel *channel);
int event_loop_update_channel(struct event_loop *eloop, struct event_channel *channel);

long long event_loop_add_timer(struct event_loop *eloop,
                                    unsigned int interval_milliseconds,
                                    enum timer_type type,
                                    event_loop_timer_proc on_timer,
                                    void *userdata);
int event_loop_remove_timer(struct event_loop *eloop, 
                            long long id);

int event_loop_add_job(struct event_loop *eloop, 
                       event_loop_job_proc on_job, 
                       void *userdata1,
                       void *userdata2,
                       void *userdata3);

#ifdef __cplusplus
}
#endif
#endif
