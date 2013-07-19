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

int read_bytes(struct ws_parser *parser, const char *data, size_t len);

static int buffer_concat(struct ws_parser *parser, const char *data, size_t len)
{
    int remaining = BUFFER_SIZE - parser->buffer_len;

    if (len > remaining)
        return -1;

    memcpy(parser->buffer + parser->buffer_len, data, len);
    parser->buffer_len += len;

    return 0;
}

int parse_frame_mask(struct ws_parser *parser)
{
    return -1;
}

int parse_frame_length(struct ws_parser *parser)
{
    return -1;
}

int parse_frame_header(struct ws_parser *parser)
{
    printf("parse_frame_header: %02x %02x\n",
        parser->buffer[0], parser->buffer[1]);

    return -1;
}

char *split_header(char *line)
{
    char *value = strchr(line, ':');
    if (value == NULL)
        return NULL;

    // null-terminate key
    *value++ = '\0';

    // remove leading spaces
    while (*value == ' ')
        value++;

    return value;
}

int parse_http_header(struct ws_parser *parser)
{
    // parser->buffer = "Header-Name: header value\r\0"

    printf("parse_http_header: %s\n", parser->buffer);

    if (!strcmp(parser->buffer, "\r")) {
        // end of header

        // send reply

        parser->remaining = 2;
        parser->parse_fn = parse_frame_header;
        parser->read_fn = read_bytes;
        return 0;
    }

    char *key = parser->buffer;
    char *value = split_header(key);
    if (value == NULL)
        return -1;

    if (!strcmp(key, "Sec-WebSocket-Key")) {
        printf("found websocket key: %s\n", value);
    }

    return 0;
}

int parse_http_get(struct ws_parser *parser)
{
    // parser->buffer = "GET /path HTTP/1.1\r\0"

    printf("parse_http_get: %s\n", parser->buffer);

    parser->parse_fn = parse_http_header;

    return 0;
}

// read a line into the buffer and then call parse_fn
int read_line(struct ws_parser *parser, const char *data, size_t len)
{
    // check if \n is in data
    int pos = 0;
    for (; pos < len; pos++)
        if (data[pos] == '\n')
            break;

    if (pos == len) {
        // no \n found, copy everything to the buffer and return
        if (buffer_concat(parser, data, pos) == -1)
            return -1;

        return len;
    }
    else {
        // \n found, copy the line to the buffer and call parse_fn

        // copy the \n char too
        pos++;

        if (buffer_concat(parser, data, pos) == -1)
            return -1;

        // null-terminate the string, getting rid of the \n
        parser->buffer[parser->buffer_len - 1] = '\0';

        // remove trailing \r
        if (parser->buffer_len > 1)
            if (parser->buffer[parser->buffer_len - 2] == '\r')
                parser->buffer[parser->buffer_len - 2] = '\0';

        if (parser->parse_fn(parser) == -1)
            return -1;

        parser->buffer_len = 0;

        return pos;
    }
}

// read n bytes into the buffer and then call parse_fn
int read_bytes(struct ws_parser *parser, const char *data, size_t len)
{
    if (len > parser->remaining)
        len = parser->remaining;
    parser->remaining -= len;

    buffer_concat(parser, data, len);

    if (parser->remaining == 0) {
        if (parser->parse_fn(parser) == -1)
            return -1;

        parser->buffer_len = 0;
    }

    return len;
}

// read n bytes, process them in chucks as they become available
int read_stream(struct ws_parser *parser, const char *data, size_t len)
{
    if (len > parser->remaining)
        len = parser->remaining;
    parser->remaining -= len;

    // send data

    // got everything, wait for next frame header
    if (parser->remaining == 0) {
        parser->remaining = 2;
        parser->parse_fn = parse_frame_header;
    }

    return len;
}

// read and parse a byte stream
// return the number of bytes consumed or -1 on error
int ws_read(struct ws_parser *parser, const char *data, size_t len)
{
    return parser->read_fn(parser, data, len);
}

struct ws_parser *ws_new()
{
    struct ws_parser *parser = malloc(sizeof(struct ws_parser));

    parser->read_fn = read_line;
    parser->parse_fn = parse_http_get;
    parser->buffer = malloc(BUFFER_SIZE);
    parser->buffer_len = 0;

    return parser;
}

void ws_free(struct ws_parser *parser)
{
    free(parser->buffer);
    free(parser);
}
