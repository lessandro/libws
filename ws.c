/*-
 * Copyright (c) 2013, Lessandro Mariano
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ws.h"

#define BUFFER_SIZE 2048
#define LF 0

static int concat(struct ws_stream *stream, const char *data, size_t len)
{
    int remaining = BUFFER_SIZE - stream->buffer_len;

    if (len > remaining)
        return 0;

    memcpy(stream->buffer + stream->buffer_len, data, len);
    stream->buffer_len += len;

    return 1;
}

void parse_frame_mask(struct ws_stream *stream)
{

}

void parse_frame_length(struct ws_stream *stream)
{

}

void parse_frame_header(struct ws_stream *stream)
{
    printf("parse_frame_header: %02x %02x\n",
        stream->buffer[0], stream->buffer[1]);
}

void parse_http_header(struct ws_stream *stream)
{
    // stream->buffer = "Header-Name: header value\r\0"

    printf("parse_http_header: %s\n", stream->buffer);

    if (!strcmp(stream->buffer, "\r")) {
        // end of header

        // send reply

        stream->wait_for = 2;
        stream->wait_func = parse_frame_header;
    }
}

void parse_http_get(struct ws_stream *stream)
{
    // stream->buffer = "GET /path HTTP/1.1\r\0"

    printf("parse_http_get: %s\n", stream->buffer);

    stream->wait_func = parse_http_header;
}

// reads a line into the buffer and then call wait_func
int read_line(struct ws_stream *stream, const char *data, size_t len)
{
    // check if \n is in data
    int pos = 0;
    for (; pos < len; pos++)
        if (data[pos] == '\n')
            break;

    if (pos == len) {
        // not in data, copy everything to the buffer and return
        if (!concat(stream, data, pos))
            return -1;

        return len;
    }
    else {
        // \n in data, copy the line to the buffer and call wait_func
        pos++;

        if (!concat(stream, data, pos))
            return -1;

        // null-terminate the string, getting rid of the \n
        stream->buffer[pos-1] = 0;

        stream->wait_func(stream);
        stream->buffer_len = 0;

        return pos;
    }
}

// reads wait_for bytes into the buffer and then call wait_func
int read_bytes(struct ws_stream *stream, const char *data, size_t len)
{
    int rest = stream->wait_for - stream->buffer_len;

    if (len < rest) {
        concat(stream, data, len);
        return len;
    }
    else {
        concat(stream, data, rest);
        stream->wait_func(stream);
        stream->buffer_len = 0;
        return rest;
    }
}

// reads wait_for bytes and process them in chucks as they come
int read_stream(struct ws_stream *stream, const char *data, size_t len)
{
    int rest = -stream->wait_for;
    if (len > rest)
        len = rest;

    // send data
    stream->wait_for += len;

    // got everything, wait for next frame header
    if (stream->wait_for == 0) {
        stream->wait_for = 2;
        stream->wait_func = parse_frame_header;
    }

    return len;
}

// returns the number of bytes consumed or -1 on error
int ws_read(struct ws_stream *stream, const char *data, size_t len)
{
    if (stream->wait_for == LF)
        return read_line(stream, data, len);
    else if (stream->wait_for > 0)
        return read_bytes(stream, data, len);
    else
        return read_stream(stream, data, len);
}

struct ws_stream *ws_new()
{
    struct ws_stream *stream = malloc(sizeof(struct ws_stream));

    stream->wait_for = LF;
    stream->wait_func = parse_http_get;
    stream->buffer = malloc(BUFFER_SIZE);
    stream->buffer_len = 0;

    return stream;
}

void ws_free(struct ws_stream *stream)
{
    free(stream->buffer);
    free(stream);
}
