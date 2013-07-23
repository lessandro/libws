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

#include <string.h>
#include "ws.h"

static int concat(struct ws_parser *parser, const char *data, size_t len)
{
    int available = WS_BUFFER_SIZE - parser->buffer_len;

    if (len > available) {
        parser->errno = WS_BUFFER_OVERFLOW;
        return -1;
    }

    if (len > 0) {
        memcpy(parser->buffer + parser->buffer_len, data, len);
        parser->buffer_len += len;
    }

    return 0;
}

// read a line into the buffer and then call parse_fn
static int read_line(struct ws_parser *parser, const char *data, size_t len)
{
    // check if \n is in data
    int pos = 0;
    for (; pos < len; pos++)
        if (data[pos] == '\n')
            break;

    int has_nl = pos < len;
    pos += has_nl;

    if (concat(parser, data, pos) == -1)
        return -1;

    if (has_nl) {
        // null-terminate the string, getting rid of the \n
        parser->buffer[parser->buffer_len - 1] = '\0';

        // remove trailing \r
        if (parser->buffer_len > 1)
            if (parser->buffer[parser->buffer_len - 2] == '\r')
                parser->buffer[parser->buffer_len - 2] = '\0';

        if (parser->parse_fn(parser) == -1)
            return -1;
    }

    return pos;
}

// read n bytes into the buffer and then call parse_fn
static int read_bytes(struct ws_parser *parser, const char *data, size_t len)
{
    if (len > parser->remaining)
        len = parser->remaining;

    if (len > 0) {
        memcpy(parser->buffer + parser->buffer_len, data, len);
        parser->buffer_len += len;
        parser->remaining -= len;
    }

    if (parser->remaining == 0)
        if (parser->parse_fn(parser) == -1)
            return -1;

    return len;
}

// read n bytes, process them in chucks as they become available
static int read_stream(struct ws_parser *parser, const char *data, size_t len)
{
    if (len > parser->remaining)
        len = parser->remaining;

    if (len > WS_BUFFER_SIZE - 1)
        len = WS_BUFFER_SIZE - 1;

    parser->result = WS_FRAME;
    parser->frame.offset += parser->buffer_len;
    parser->frame.len = len;

    for (int i = 0; i < len; i++)
        parser->frame.data[i] = data[i] ^ parser->frame.mask[i % 4];

    parser->remaining -= len;

    if (parser->remaining == 0)
        parser->parse_fn(parser);

    return len;
}

void read_stream_cb(struct ws_parser *parser, uint64_t len, ws_callback *fn)
{
    parser->remaining = len;
    parser->buffer_len = 0;
    parser->read_fn = read_stream;
    parser->parse_fn = fn;
}

void read_bytes_cb(struct ws_parser *parser, size_t len, ws_callback *fn)
{
    parser->remaining = len;
    parser->buffer_len = 0;
    parser->read_fn = read_bytes;
    parser->parse_fn = fn;
}

void read_line_cb(struct ws_parser *parser, ws_callback *fn)
{
    parser->buffer_len = 0;
    parser->read_fn = read_line;
    parser->parse_fn = fn;
}
