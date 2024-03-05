#ifndef __TCP_SERVER_H__
#define __TCP_SERVER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tcp_connect.h"

#include "event_loop_pool.h"
#include "event_channel.h"

struct tcp_server;

struct tcp_server *tcp_server_open(const char *addr, 
                                        unsigned short port, 
                                        int backlog, 
                                        struct event_loop_pool *e_pool, 
                                        tcp_connect_proc new_proc,
                                        tcp_connect_proc read_proc,
                                        tcp_connect_proc write_proc,
                                        tcp_connect_proc close_proc,
                                        void *proc_userdata,
                                        char *err, size_t err_length);
void tcp_server_close(struct tcp_server **serverp);

int tcp_server_get_fd(struct tcp_server *server);
struct event_channel *tcp_server_get_channel(struct tcp_server *server);

#ifdef __cplusplus
}
#endif
#endif
