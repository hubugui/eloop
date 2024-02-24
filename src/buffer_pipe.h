#ifndef __BUFFER_PIPE_H__
#define __BUFFER_PIPE_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct buffer_pipe;

struct buffer_pipe *buffer_pipe_create(void);
void buffer_pipe_delete(struct buffer_pipe **pipe_p);

size_t buffer_pipe_get_length(struct buffer_pipe *pipe);
int buffer_pipe_expand(struct buffer_pipe *pipe, size_t length);

int buffer_pipe_write(struct buffer_pipe *pipe, char *data, size_t length);
int buffer_pipe_write_head(struct buffer_pipe *pipe, char *data, size_t length);

size_t buffer_pipe_read(struct buffer_pipe *pipe, char *data, size_t length);
int buffer_pipe_read_line(struct buffer_pipe *pipe, char *line, size_t line_len, size_t *real_len);

int buffer_pipe_find_chr(struct buffer_pipe *pipe, char mark, size_t *pos);

#ifdef __cplusplus
}
#endif
#endif
