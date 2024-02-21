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
};

static int 
_on_tcp_accept(struct event_channel *channel)
{
    int ret = 0;
    char ip[256] = {0};
    char err[256] = {0};
    unsigned short port = 0;
    int tcp_server_fd = event_channel_get_fd(channel);
    struct tcp_server *server = (struct tcp_server *) event_channel_get_userdata(channel);

    int client_fd = net_tcp_accept(tcp_server_fd, ip, sizeof(ip), &port, err, sizeof(err));
    if (client_fd != -1) {
        struct event_loop *e_loop = event_loop_pool_next(server->e_pool);
        struct tcp_connect *connect = tcp_connect_create(client_fd, e_loop, _on_client_read, _on_client_write, _on_client_close);
        if (server->new_proc)   server->new_proc(connect);

        printf("%s>%d>accept(%d) client fd=%d, ip=%s, port=%d, connect=%p\n", __FUNCTION__, __LINE__, tcp_server_fd, client_fd, ip, port, connect);
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
                    char *err, 
                    size_t err_length)
{
    struct tcp_server *server = (struct tcp_server *) calloc(1, sizeof(*server));
    struct event_loop *e_loop;
    int ret = 0;

    if (server) goto EXIT;

    server->new_proc = new_proc;

    server->fd = net_tcp_server(addr, port, backlog, err, err_length);
    if (server->fd == -1) {
        tcp_server_close(&server);
        goto EXIT;
    }

    server->channel = event_channel_create();
    if (!server->channel) {
        tcp_server_close(&server);
        goto EXIT;
    }
    event_channel_set_fd(server->channel, tcp_server_get_fd(server));
    event_channel_add_mask(server->channel, FD_MASK_READ);
    event_channel_set_userdata(server->channel, server);
    event_channel_set_read_proc(server->channel, _on_tcp_accept);

    e_loop = event_loop_pool_next(e_pool);

    ret = event_loop_add_channel(e_loop, server->channel);
    if (ret) {
        tcp_server_close(&server);
        goto EXIT;
    }

    server->e_loop = e_loop;
    server->e_pool = e_pool;
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
