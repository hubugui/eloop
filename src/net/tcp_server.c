/*
 * tcp server
 *
 * Copyright (c) 2023 hubugui <hubugui at gmail dot com> 
 * All rights reserved.
 *
 * This file is part of Eloop.
 */

#include <string.h>
#include <stdlib.h>

#include "tcp_server.h"
#include "net.h"
#include "event_channel.h"

struct tcp_server {
    struct event_loop_pool *e_pool;
    struct event_loop *e_loop;
    struct event_channel *channel;
    int fd;

    tcp_connect_proc new_proc;
    tcp_connect_proc procs[PROC_END_OF];
};

static int 
_on_tcp_accept(struct event_channel *channel)
{
    int ret = 0;
    char ipv4[256] = {0};
    char err[256] = {0};
    unsigned short port = 0;
    int tcp_server_fd = event_channel_get_fd(channel);
    struct tcp_server *server = (struct tcp_server *) event_channel_get_userdata(channel);

    int client_fd = net_tcp_accept(tcp_server_fd, ipv4, sizeof(ipv4), &port, err, sizeof(err));
    if (client_fd != -1) {
        struct event_loop *e_loop = event_loop_pool_next(server->e_pool);
        struct tcp_connect *connect = tcp_connect_create(client_fd, ipv4, strlen(ipv4), port, e_loop, server->procs[PROC_READ]
                                                        , server->procs[PROC_WRITE], server->procs[PROC_CLOSE]);

        if (server->new_proc)   server->new_proc(connect);
    } else {
        printf("%s>%d>accept(%d) fail, error=\"%s\"\n", __FUNCTION__, __LINE__, tcp_server_fd, err);
        ret = -1;
    }

    return ret;
}

struct tcp_server *
tcp_server_open(const char *addr, 
                    unsigned short port, 
                    int backlog, 
                    struct event_loop_pool *e_pool,
                    tcp_connect_proc new_proc,
                    tcp_connect_proc read_proc,
                    tcp_connect_proc write_proc,
                    tcp_connect_proc close_proc,
                    char *err, 
                    size_t err_length)
{
    struct tcp_server *server = (struct tcp_server *) calloc(1, sizeof(*server));
    struct event_loop *e_loop;

    if (!server)            goto ERROR;

    server->e_pool = e_pool;
    server->new_proc = new_proc;    
    server->procs[PROC_READ] = read_proc;
    server->procs[PROC_WRITE] = write_proc;
    server->procs[PROC_CLOSE] = close_proc;

    server->fd = net_tcp_server(addr, port, backlog, err, err_length);
    if (server->fd == -1)   goto ERROR;

    server->channel = event_channel_create();
    if (!server->channel)   goto ERROR;
    event_channel_set_fd(server->channel, tcp_server_get_fd(server));
    event_channel_add_mask(server->channel, FD_MASK_READ);
    event_channel_set_userdata(server->channel, server);
    event_channel_set_read_proc(server->channel, _on_tcp_accept);

    e_loop = event_loop_pool_next(e_pool);
    if (event_loop_add_channel(e_loop, server->channel))    goto ERROR;

    server->e_loop = e_loop;
    goto EXIT;
ERROR:
    tcp_server_close(&server);
EXIT:
    return server;
}

void 
tcp_server_close(struct tcp_server **serverp)
{
    struct tcp_server *server = serverp && (*serverp) ? (*serverp) : NULL;

    if (!server)                            return;
    if (server->e_loop && server->channel)  event_loop_remove_channel(server->e_loop, server->channel);
    if (server->channel)                    event_channel_delete(&server->channel);

    net_fd_close(server->fd);

    free(server);
    *serverp = NULL;
}

int 
tcp_server_get_fd(struct tcp_server *server)
{
    return server->fd;
}

struct event_channel *
tcp_server_get_channel(struct tcp_server *server)
{
    return server->channel;
}
