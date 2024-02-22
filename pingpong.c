/*
 * ping pong
 *
 * Copyright (c) 2023 hubugui <hubugui at gmail dot com> 
 * All rights reserved.
 *
 * This file is part of Eloop.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "buffer_pipe.h"
#include "event_loop_pool.h"

#include "net/net.h"
#include "net/tcp_connect.h"
#include "net/tcp_server.h"

#define PINGPONG_PORT   "14317"

static void 
_print_args_wrong()
{
    printf("please type -h to known right arguments\n");
}

static void 
_print_help()
{
    const char *helps = {
        "Usage:     pingpong [-s port|-c host]\n" \
        "           pingpong [-h|--help]\n" \
        "           pingpong, default is tcp server and listen to 14317\n"
    };

    printf("%s", helps);
}

static int 
_options_parse(int argc, char *argv[], int *is_server, unsigned short *port, char **host)
{
    int ret = 0;
	int i;

    for (i = 1; i < argc; ) {
        if (strcmp(argv[i], "-s") == 0) {
            if (i >= argc || argv[i+1][0] == '-') {
                i += 1;
            } else {
                *port = atoi(argv[i+1]);
                i += 2;
            }    
        } else if (strcmp(argv[i], "-c") == 0) {
            *is_server = 0;

            if (i >= argc || argv[i+1][0] == '-') {
                i += 1;
            } else {
                *host = argv[i+1];
                i += 2;
            }
        } else if (strcmp(argv[i], "-h") == 0) {
            _print_help();
            exit(0);
        } else {
            ret = -1;
            break;
        }        
    }

    return ret;
}

static int
_run_console()
{
    int ret = 0;
    char cmd[256];

    for (;;) {
        fgets(cmd, sizeof(cmd), stdin);
        if (strcmp(cmd, "exit") == 0)
            return ret;
    }

    return ret;
}

static int 
_on_timer_proc(struct event_loop *e_loop, 
                    long long id, 
                    void *userdata)
{
  printf("%s>%d>id=%lld\n", __FUNCTION__, __LINE__, id);
  return 0;
}

static int 
_on_job_proc(struct event_loop *eloop, 
                    void *userdata1,
                    void *userdata2,
                    void *userdata3)
{
    printf("%s>%d>userdata1=%p, userdata2=%p, userdata3=%p\n", __FUNCTION__, __LINE__, userdata1, userdata2, userdata3);
    return 0;
}

static int 
_on_client_close(struct tcp_connect *connect)
{
    struct event_channel *channel = tcp_connect_get_event_channel(connect);
    int fd = event_channel_get_fd(channel);

    printf("%s>%d>fd=%d\n", __FUNCTION__, __LINE__, fd);

    tcp_connect_delete(&connect);
    return 0;
}

static int 
_on_client_write(struct tcp_connect *connect)
{
    printf("%s>%d>\n", __FUNCTION__, __LINE__);

    if (tcp_connect_write(connect) == -1)
        _on_client_close(connect);
    return 0;
}

static int 
_on_client_read(struct tcp_connect *connect)
{
    int ret = 0;
    size_t pos = 0;
    char buffer[1024];
    struct event_channel *channel = tcp_connect_get_event_channel(connect);
    struct buffer_pipe *pipe_recv = event_channel_get_recv_pipe(channel);
    struct buffer_pipe *pipe_send = event_channel_get_send_pipe(channel);

    printf("%s>%d>\n", __FUNCTION__, __LINE__);

    /* parse */
    ret = buffer_pipe_find_chr(pipe_recv, '\n', &pos);
    if (ret == 0) {
        buffer_pipe_read(pipe_recv, buffer, pos + 1);
        buffer[pos] = '\0';
        if (pos > 0 && buffer[pos - 1] == '\r')
            buffer[pos - 1] = '\0';

        if (strcmp(buffer, "ping") == 0) {
            /* pong */
            buffer_pipe_write(pipe_send, "pong\n", 5);
            tcp_connect_mark_write(connect);
        } else if (strcmp(buffer, "exit") == 0) {
            /* close */
            _on_client_close(connect);
            ret = 1;
        }
    } else
        ret = -1;

    return ret;
}

static int 
_on_client_new(struct tcp_connect *connect)
{
    struct event_channel *channel = tcp_connect_get_event_channel(connect);
    int fd = event_channel_get_fd(channel);
    char ipv4[512];
    unsigned short port = tcp_connect_get_port(connect);
    tcp_connect_get_ipv4(connect, ipv4, sizeof(ipv4));

    printf("%s>%d>client fd=%d, ipv4=%s, port=%d, connect=%p\n", __FUNCTION__, __LINE__, fd, ipv4, port, connect);
    return 0;
}

static int
_run_server(unsigned short port)
{
    int ret = 0;
    struct tcp_server *server;
    long long timer_id;
    char err[256] = {0};

    struct event_loop_pool *e_pool = event_loop_pool_create(8);
    if (!e_pool) {
        ret = -1;
        goto EXIT;
    }

    server = tcp_server_open("0.0.0.0", port, 1000, e_pool, _on_client_new, _on_client_read, _on_client_write, _on_client_close, err, sizeof(err));
    if (!server) {
        event_loop_pool_delete(&e_pool);
        ret = -2;
        goto EXIT;
    }

    printf("%s>%d>listen to %d\n", __FUNCTION__, __LINE__, port);

    timer_id = event_loop_add_timer(event_loop_pool_next(e_pool), 5000, timer_type_forever, _on_timer_proc, server);
    if (timer_id < 1) {
        tcp_server_close(&server);
        event_loop_pool_delete(&e_pool);
        ret = -3;
        goto EXIT;
    }

    event_loop_add_job(event_loop_pool_next(e_pool), _on_job_proc, 1, 2, 3);

    ret = _run_console();
EXIT:
    return ret;
}

static int
_run_client(char *host)
{
   return 0;
}

int 
main(int argc, char *argv[])
{
    int ret = 0;
    int is_server = 1;
    unsigned short port = atoi(PINGPONG_PORT);
    char *host = "0.0.0.0:"PINGPONG_PORT;

    if (argc > 1) {
        ret = _options_parse(argc, argv, &is_server, &port, &host);
        if (ret) {
            _print_args_wrong();
            return 1;
        }
    }

    ret = net_init();
    if (ret) {
        printf("%s>%d>net_init() fail=%d\n", __FUNCTION__, __LINE__, ret);
        ret = 2;    
        goto EXIT;
    }

    ret = is_server ? _run_server(port) : _run_client(host);
    if (ret) {
        printf("%s>%d>%s() fail=%d\n", __FUNCTION__, __LINE__, is_server ? "_run_server" : "_run_client", ret);
        ret = 3;
    }        
EXIT:
    net_finalize();
    return ret;
}
