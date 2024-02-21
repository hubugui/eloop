/*
 * net
 *
 * Copyright (c) 2023 hubugui <hubugui at gmail dot com>
 * 
 * All rights reserved.
 *
 * This file is part of Eloop.
 */

#include <string.h>
#include <stdlib.h>

#if defined(__linux) || defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <errno.h>
#elif defined(WIN32) || defined(_WIN32) 
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <stdio.h>

#include "net.h"

int 
net_init()
{
    int ret = 0;

#ifdef WIN32
    WSADATA wsa = {0};
    ret = WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    return ret;
}

void 
net_finalize()
{
#ifdef WIN32
    WSACleanup();
#endif
}

int 
net_tcp_connect(const char *addr, unsigned short port, char *err, size_t err_length)
{
    int fd = 0;
    struct sockaddr_in socket_addr = {0};
    socklen_t addrlen = sizeof(socket_addr);

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) return -1;

    socket_addr.sin_family = AF_INET;
    socket_addr.sin_port = htons(port);
    socket_addr.sin_addr.s_addr = inet_addr(addr);
    if (connect(fd, (struct sockaddr*)&addr, addrlen) == -1)
        net_fd_close(&fd);

    return fd;
}

int 
net_fd_close(int *fd)
{
    int ret = 0;

    if (!fd)        goto EXIT;
    if (*fd == -1)  goto EXIT;

#if defined(__linux) || defined(__linux__)  || defined(__APPLE__) || defined(__FreeBSD__)
    close(*fd);
#elif defined(WIN32) || defined(_WIN32)
    closesocket(*fd);      
#endif

    *fd = -1;
EXIT:    
    return ret;
}

int
net_fd_shutdown_read(int fd)
{
#if defined(__linux) || defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
    return shutdown(fd, SHUT_RD);
#elif defined(WIN32) || defined(_WIN32)
    return shutdown(fd, SD_RECEIVE);
#endif
}

int
net_fd_shutdown_write(int fd)
{
#if defined(__linux) || defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
    return shutdown(fd, SHUT_WR);
#elif defined(WIN32) || defined(_WIN32)
    return shutdown(fd, SD_SEND);
#endif
}

int 
net_fd_set_block_direct(int fd, char is_block, int *error)
{
    int ret = 0;

#if defined(__linux) || defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
    int flags = fcntl(fd, F_GETFL, 0);
    if (is_block)   flags = flags & ~O_NONBLOCK;
    else            flags = flags | O_NONBLOCK;
    ret = fcntl(fd, F_SETFL, flags);
#else
    u_long nb = is_block ? 0 : 1;
#ifdef __GNUC__
    int cmd = 0x8004667E;
#else
    int cmd = FIONBIO;
#endif
    ret = ioctlsocket(fd, cmd, &nb);
    if (ret == SOCKET_ERROR)
        *error = net_get_last_error();
#endif

    return ret;
}

int 
net_fd_set_block(int fd, int *error)
{
    return net_fd_set_block_direct(fd, 1, error);
}

int 
net_fd_set_noblock(int fd, int *error)
{
    return net_fd_set_block_direct(fd, 0, error);
}

int 
net_fd_read(int fd, char *buffer, size_t length, int *error)
{
    int ret = 0;

    *error = 0;

#if defined(__linux) || defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
    ret = recv(fd, buffer, length, 0);
    if (ret == -1) {
        *error = errno;
    }
    /* TODO: process no data */
#else
    ret = recv(fd, buffer, (int) length, 0);
    if (ret == SOCKET_ERROR) {
        *error = WSAGetLastError();
        if (*error == WSAEWOULDBLOCK)
            *error = EAGAIN;
    }
#endif

    return ret;
}

int 
net_fd_write(int fd, char *buffer, size_t length)
{
#if defined(__linux) || defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
    return send(fd, buffer, length, 0);
#else
    return send(fd, buffer, (int) length, 0);
#endif
}

int 
net_tcp_set_reuse(int fd)
{
    int value = 1;

    if (setsockopt(fd, IPPROTO_TCP, SO_REUSEADDR, &value, sizeof(value)) == -1)
        return -1;
    return 0;
}

int 
net_tcp_set_delay(int fd)
{
    int value = 0;

    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &value, sizeof(value)) == -1)
        return -1;
    return 0;
}

int 
net_tcp_set_nodelay(int fd)
{
    int value = 1;

    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &value, sizeof(value)) == -1)
        return -1;
    return 0;
}

int 
net_tcp_server(const char *addr, unsigned short port, int backlog, char *err, size_t err_length)
{
    struct sockaddr_in socket_addr = {0};
    int ret = 0, error = 0;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {        
        memset(err, 0x00, err_length);
        error = net_get_last_error();
        snprintf(err, err_length, "socket fail, error=%d", error);
        return -1;
    }

    ret = net_fd_set_noblock(fd, &error);
    if (ret) {
        snprintf(err, err_length, "set_noblock fail, error=%d", error);
        net_fd_close(&fd);
        goto EXIT;
    }
    net_tcp_set_reuse(fd);

    socket_addr.sin_family = AF_INET;
    socket_addr.sin_addr.s_addr = inet_addr(addr);
    socket_addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&socket_addr, sizeof(socket_addr)) == -1) {
        error = net_get_last_error();
        snprintf(err, err_length, "bind fail, error=%d", error);
        net_fd_close(&fd);
        goto EXIT;
    }

    if (listen(fd, backlog) == -1) {
        error = net_get_last_error();
        snprintf(err, err_length, "listen fail, error=%d", error);
        net_fd_close(&fd);
        goto EXIT;
    }

EXIT:
    return fd;
}

int 
net_tcp_accept(int fd, char *ip, size_t ip_length, unsigned short *port, char *err, size_t err_length)
{
    struct sockaddr_in addr = {0};
    socklen_t addrlen = sizeof(addr);
    int client_fd = accept(fd, (struct sockaddr*) &addr, &addrlen);
    int error = 0;

LOOP:
    if (client_fd != -1) {
        if (net_fd_set_noblock(client_fd, &error)) {
            net_fd_close(&client_fd);
            client_fd = -1;
            goto LOOP;
        }

#if defined(__linux) || defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
        inet_ntop(AF_INET, &addr.sin_addr, ip, ip_length);
#elif defined(WIN32) || defined(_WIN32)
        char *peer_ip = inet_ntoa(addr.sin_addr);
        if (peer_ip)  strncpy(ip, peer_ip, ip_length);
#endif

        *port = addr.sin_port;
    } else {
#if defined(__linux) || defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
        snprintf(err, err_length, "client_fd=%d, error=%d", client_fd, error);
#elif defined(WIN32) || defined(_WIN32)
        snprintf(err, err_length, "client_fd=%d, error=%d", client_fd, error);
#endif
    }

    return client_fd;
}

int net_get_last_error()
{
    int ret = 0;

#if defined(__linux) || defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
    if (errno == EINTR || errno == EAGAIN)
        ret = EAGAIN;
#elif defined(WIN32) || defined(_WIN32)
    int error = WSAGetLastError();
    if (error == WSAEWOULDBLOCK || error == WSAEINPROGRESS || error == 0)
        ret = EAGAIN;
    else
        ret = error;
#endif
    return ret;
}
