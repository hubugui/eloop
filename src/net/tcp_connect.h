#ifndef __TCP_CONNECT_H__
#define __TCP_CONNECT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "../event_loop.h"
#include "../event_channel.h"

struct tcp_connect;

typedef int (*tcp_connect_proc)(struct tcp_connect *connect, void *userdata);

struct tcp_connect *tcp_connect_create(int fd, 
                                            char *ipv4,
                                            size_t ipv4_length,                                            
                                            unsigned short port,
                                            struct event_loop *e_loop, 
                                            tcp_connect_proc read_proc, 
                                            tcp_connect_proc write_proc, 
                                            tcp_connect_proc close_proc,
                                            void *proc_userdata);
void tcp_connect_delete(struct tcp_connect **connectp);

int tcp_connect_write(struct tcp_connect *connect);

int tcp_connect_mark_read(struct tcp_connect *connect);

int tcp_connect_mark_write(struct tcp_connect *connect);
int tcp_connect_unmark_write(struct tcp_connect *connect);

struct event_loop *tcp_connect_get_event_loop(struct tcp_connect *connect);
struct event_channel *tcp_connect_get_event_channel(struct tcp_connect *connect);

void tcp_connect_set_userdata(struct tcp_connect *connect, void *userdata);
void *tcp_connect_get_userdata(struct tcp_connect *connect);

unsigned short tcp_connect_get_port(struct tcp_connect *connect);
char *tcp_connect_get_ipv4(struct tcp_connect *connect, char *ipv4, size_t length);

#ifdef __cplusplus
}
#endif
#endif
