/*
 * buffer pipe
 *
 * Copyright (c) 2023 hubugui <hubugui at gmail dot com> 
 * All rights reserved.
 *
 * This file is part of Eloop.
 */

#include <string.h>
#include <stdlib.h>

#include "buffer_pipe.h"

#define BUCKET_LENGTH  4096

struct buffer_pipe {
    char *data;
    size_t length;
    size_t actual_length;
};

struct buffer_pipe *
buffer_pipe_create(void)
{
    return (struct buffer_pipe *) calloc(1, sizeof(struct buffer_pipe));
}

void 
buffer_pipe_delete(struct buffer_pipe **pipe_p)
{
    struct buffer_pipe *pipe = pipe_p && (*pipe_p) ? (*pipe_p) : NULL;

    if (!pipe)        return;
    if (pipe->data)   free(pipe->data);
    free(pipe);
    *pipe_p = NULL;
}

size_t 
buffer_pipe_get_length(struct buffer_pipe *pipe)
{
    return pipe->length;
}

int 
buffer_pipe_expand(struct buffer_pipe *pipe, size_t length)
{
    int ret = 0;
    size_t new_length = pipe->length + length + BUCKET_LENGTH;
    char *new_addr = realloc(pipe->data, new_length);

    if (new_addr) {
        pipe->data = new_addr;
        pipe->actual_length = new_length;
    } else
        ret = -1;

    return ret;
}

int 
buffer_pipe_write(struct buffer_pipe *pipe, char *data, size_t length)
{
    int ret = 0;

    if (pipe->length + length > pipe->actual_length)
        ret = buffer_pipe_expand(pipe, length);

    if (ret == 0) {
        memmove(pipe->data + pipe->length, data, length);
        pipe->length += length;
    }

    return ret;
}

int 
buffer_pipe_write_head(struct buffer_pipe *pipe, char *data, size_t length)
{
    int ret = 0;

    if (pipe->length + length > pipe->actual_length)
        ret = buffer_pipe_expand(pipe, length);

    if (ret == 0) {
        /* move back */
        memmove(pipe->data + length, pipe->data, pipe->length);
        /* head */
        memmove(pipe->data, data, length);
        pipe->length += length;
    }

    return ret;
}

size_t 
buffer_pipe_read(struct buffer_pipe *pipe, char *data, size_t length)
{
    size_t ret = pipe->length < length ? pipe->length : length;

    if (ret > 0) {
        memmove(data, pipe->data, ret);
        memmove(pipe->data, pipe->data + ret, pipe->length - ret);
        pipe->length -= ret;
    }

    return ret;
}

int 
buffer_pipe_read_line(struct buffer_pipe *pipe, char *line, size_t line_len, size_t *real_len)
{
    int ret = -2;
    size_t pos = 0;

    *real_len = 0;
    while (pos < line_len) {
        size_t read_len = buffer_pipe_read(pipe, line + pos, 1);

        if (read_len == 0) {
            ret = -1;
            break;
        }

        if (line[pos] == '\r') {
            ;
        } else if (line[pos] == '\n') {
            line[pos] = '\0';
            ret = 0;
            *real_len = pos;
            break;
        } else
            pos++;
    }

    return ret;
}

int 
buffer_pipe_find_chr(struct buffer_pipe *pipe, char mark, size_t *pos)
{
    int ret = -1;

    for (size_t i = 0; i < pipe->length; i++) {
        if (pipe->data[i] == mark) {
            *pos = i;
            ret = 0;
            break;
        }
    }

    return ret;
}
