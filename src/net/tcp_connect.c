/*
 * tcp connect
 *
 * Copyright (c) 2023 hubugui <hubugui at gmail dot com> 
 * All rights reserved.
 *
 * This file is part of Eloop.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "tcp_connect.h"
#include "net.h"

struct tcp_connect {
    char ipv4[512];
    unsigned short port;

    struct event_channel *channel;
    struct event_loop *e_loop;
    tcp_connect_proc procs[PROC_END_OF];
    void *proc_userdata;

    void *userdata;
};

static int 
_tcp_connec_on_close(struct event_channel *channel)
{
    struct tcp_connect *connect = (struct tcp_connect *) event_channel_get_userdata(channel);

    if (connect->procs[PROC_CLOSE]) connect->procs[PROC_CLOSE](connect, connect->proc_userdata);
    else                            tcp_connect_delete(&connect);
    return 0;
}

static int 
_tcp_connec_on_write(struct event_channel *channel)
{
    struct tcp_connect *connect = (struct tcp_connect *) event_channel_get_userdata(channel);

    if (connect->procs[PROC_WRITE]) connect->procs[PROC_WRITE](connect, connect->proc_userdata);
    return 0;
}

static int 
_tcp_connec_on_read(struct event_channel *channel)
{
    int ret = 0;
    char buffer[1024];
    int fd = event_channel_get_fd(channel);
    struct tcp_connect *connect = (struct tcp_connect *) event_channel_get_userdata(channel);
    struct buffer_pipe *pipe_recv = event_channel_get_recv_pipe(channel);
    char reading = 1;
    char has_data = 0;
    char need_close = 0;
    int error = 0;

    while (reading) {
        /* read from socket */
        ret = net_fd_read(fd, buffer, sizeof(buffer), &error);
        if (ret > 0) {
            /* write */
            ret = buffer_pipe_write(pipe_recv, buffer, (size_t) ret);
            if (ret == 0)
                has_data = 1;
            else {
                reading = 0;
                need_close = 1;
            }
        } else if (ret == 0) {
            reading = 0;
            need_close = 1;
        } else {
            if (error != EAGAIN) {
                need_close = 1;
            }
            reading = 0;
        }
    }

    if (has_data == 1) {
        if (connect->procs[PROC_READ])  ret = connect->procs[PROC_READ](connect, connect->proc_userdata);
        if (ret == 1)                   need_close  = 0;
    }

    if (need_close == 1)
        _tcp_connec_on_close(channel);

    if (ret < 0)
        ret = -1;
    return ret;
}

struct tcp_connect *
tcp_connect_create(int fd, 
                        char *ipv4,
                        size_t ipv4_length,
                        unsigned short port,
                        struct event_loop *e_loop, 
                        tcp_connect_proc read_proc, 
                        tcp_connect_proc write_proc, 
                        tcp_connect_proc close_proc,
                        void *proc_userdata)
{
    struct tcp_connect *connect = (struct tcp_connect *) calloc(1, sizeof(*connect));

    if (connect) {
        struct event_channel *channel = event_channel_create();

        if (!channel) {
            tcp_connect_delete(&connect);
            goto EXIT;
        }
        connect->channel = channel;
        connect->e_loop = e_loop;
        connect->procs[PROC_READ] = read_proc;
        connect->procs[PROC_WRITE] = write_proc;
        connect->procs[PROC_CLOSE] = close_proc;
        connect->proc_userdata = proc_userdata;

        strncpy(connect->ipv4, ipv4, ipv4_length);
        connect->port = port;

        event_channel_set_fd(channel, fd);
        event_channel_set_userdata(channel, connect);
        tcp_connect_mark_read(connect);

        if (event_loop_add_channel(e_loop, channel) != 0) {
            connect->e_loop = NULL;
            tcp_connect_delete(&connect);
        }
    }

EXIT:
    return connect;
}

void 
tcp_connect_delete(struct tcp_connect **connectp)
{
    if (connectp && *connectp) {
        struct tcp_connect *connect = *connectp;
        int fd = event_channel_get_fd(connect->channel);

        if (connect->e_loop && connect->channel)
            event_loop_remove_channel(connect->e_loop, connect->channel);
        if (connect->channel) {
            net_fd_close(&fd);
            event_channel_delete(&(connect->channel));
        }
        free(connect);
        *connectp = NULL;
    }
}

int 
tcp_connect_write(struct tcp_connect *connect)
{
    int ret = 0;
    int remain_data = 1;
    int close_fd = 0;
    char buffer[1024] = {0};
    struct event_channel *channel = tcp_connect_get_event_channel(connect);
    struct event_loop *e_loop = (struct event_loop *) event_channel_get_userdata(channel);
    struct buffer_pipe *pipe_send = event_channel_get_send_pipe(channel);

    while (1) {
        size_t length = buffer_pipe_read(pipe_send, buffer, sizeof(buffer));

        if (length > 0) {
            ret = net_fd_write(event_channel_get_fd(channel), buffer, length);
            if (ret > 0) {
                size_t actual_write_len = (size_t) ret;
                if (actual_write_len < length) {
                    /* write remain data to head and wait for next write */
                    buffer_pipe_write_head(pipe_send, buffer + actual_write_len, length - actual_write_len);
                    break;
                }
            } else if (ret == 0) {
                close_fd = 1;
                break;
            } else {
                ret = net_get_last_error();
                if (ret != EAGAIN) {
                    close_fd = 1;
                    break;
                }
            }
        } else {
            remain_data = 0;
            break;
        }
    }

    if (close_fd)
        ret = -1;
    else {
        if (remain_data == 0)
            tcp_connect_unmark_write(connect);
    }

    return ret;
}

int 
tcp_connect_mark_read(struct tcp_connect *connect)
{
    event_channel_add_mask(connect->channel, FD_MASK_READ | FD_MASK_ERROR);
    event_channel_set_read_proc(connect->channel, _tcp_connec_on_read);
    event_channel_set_close_proc(connect->e_loop, _tcp_connec_on_close);  
    return 0;
}

int 
tcp_connect_mark_write(struct tcp_connect *connect)
{
    event_channel_add_mask(connect->channel, FD_MASK_WRITE);
    event_channel_set_write_proc(connect->channel, _tcp_connec_on_write);
    event_loop_update_channel(connect->e_loop, connect->channel);
    return 0;
}

int 
tcp_connect_unmark_write(struct tcp_connect *connect)
{
    event_channel_remove_mask(connect->channel, FD_MASK_WRITE);
    event_channel_set_write_proc(connect->channel, NULL);
    event_loop_update_channel(connect->e_loop, connect->channel);
    return 0;
}

struct event_loop *
tcp_connect_get_event_loop(struct tcp_connect *connect)
{
    return connect->e_loop;
}

struct event_channel *
tcp_connect_get_event_channel(struct tcp_connect *connect)
{
    return connect->channel;
}

void 
tcp_connect_set_userdata(struct tcp_connect *connect, void *userdata)
{
    connect->userdata = userdata;
}

void *
tcp_connect_get_userdata(struct tcp_connect *connect)
{
    return connect->userdata;
}

unsigned short 
tcp_connect_get_port(struct tcp_connect *connect)
{
    return connect->port;
}

char *
tcp_connect_get_ipv4(struct tcp_connect *connect, char *ipv4, size_t length)
{
    strncpy(ipv4, connect->ipv4, length);
    return ipv4;
}
