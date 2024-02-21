#ifndef __EVENT_IO_H__
#define __EVENT_IO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>

struct event_io;
struct event_channel_map;
struct event_channel;

struct event_io *event_io_create(void);
void event_io_delete(struct event_io **eiop);
int event_io_add_fd(struct event_io *eio, struct event_channel *channel);
int event_io_remove_fd(struct event_io *eio, struct event_channel *channel);
int event_io_poll(struct event_io *eio, struct event_channel_map *ec_map, unsigned long long timeout);

#ifdef __cplusplus
}
#endif

#endif // __EVENT_IO_H__
