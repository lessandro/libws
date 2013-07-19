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
#include "sha1.h"
#include "base64.h"

#define BUFFER_SIZE 2048

static int read_bytes(struct ws_parser *parser, const char *data, size_t len);
static int read_stream(struct ws_parser *parser, const char *data, size_t len);

static int concat(struct ws_parser *parser, const char *data, size_t len)
{
    int remaining = BUFFER_SIZE - parser->buffer_len;

    if (len > remaining)
        return -1;

    memcpy(parser->buffer + parser->buffer_len, data, len);
    parser->buffer_len += len;

    return 0;
}

static int parse_frame_mask(struct ws_parser *parser)
{
    parser->read_fn = read_stream;
    parser->remaining = parser->frame_len;

    for (int i=0; i<4; i++)
        parser->mask[i] = parser->buffer[i];

    return 0;
}

static int parse_frame_length(struct ws_parser *parser)
{
    unsigned char *p = (unsigned char *)parser->buffer;

    if (parser->frame_len == 126) {
        parser->frame_len =
            ((unsigned long long) p[0] << 8) |
            ((unsigned long long) p[1] << 0);
    } else {
        parser->frame_len =
            ((unsigned long long) p[0] << 56) |
            ((unsigned long long) p[1] << 48) |
            ((unsigned long long) p[2] << 40) |
            ((unsigned long long) p[3] << 32) |
            ((unsigned long long) p[4] << 24) |
            ((unsigned long long) p[5] << 16) |
            ((unsigned long long) p[6] << 8) |
            ((unsigned long long) p[7] << 0);
    }

    parser->remaining = 4;
    parser->parse_fn = parse_frame_mask;

    return 0;
}

static int parse_frame_header(struct ws_parser *parser)
{
    parser->data = NULL;
    parser->data_len = 0;
    parser->data_offset = 0;

    int opcode = parser->buffer[0] & 0x0f;
    if (opcode != 1 && opcode != 2)
        return -1;

    int len = parser->buffer[1] & 0x7f;
    parser->frame_len = len;

    int has_mask = !!(parser->buffer[1] & 0x80);

    if (!has_mask)
        return -1;

    if (len < 126) {
        parser->remaining = 4;
        parser->parse_fn = parse_frame_mask;
    } else {
        parser->remaining = len == 126 ? 2 : 8;
        parser->parse_fn = parse_frame_length;
    }

    return 0;
}

#define GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define GUID_LEN 36

static void compute_challenge(const char *key, char *encoded)
{
    char buf[GUID_LEN + strlen(key) + 1];

    buf[0] = '\0';
    strcat(buf, key);
    strcat(buf, GUID);

    unsigned char result[SHA1_RESULTLEN];
    struct sha1_ctxt ctx;
    sha1_init(&ctx);
    sha1_loop(&ctx, (unsigned char *)buf, strlen(buf));
    sha1_result(&ctx, (char *)result);

    base64_encode(encoded, (unsigned char *)result, SHA1_RESULTLEN);
}

static const char *http_reply =
    "HTTP/1.1 101 Switching Protocols\r\n"
    "Upgrade: websocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Accept: ";

void ws_http_reply(struct ws_parser *parser)
{
    char encoded[base64_encode_len(SHA1_RESULTLEN)];

    compute_challenge(parser->key, encoded);

    strcpy(parser->buffer, http_reply);
    strcat(parser->buffer, encoded);
    strcat(parser->buffer, "\r\n\r\n");
}

// split an http header line ("key: value")
// return a pointer to the value or NULL if ':' is not in the string
// zero-length keys and values are allowed
static char *split_header(char *line)
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

// parse one http header line
static int parse_http_header(struct ws_parser *parser)
{
    printf("parse_http_header: %s\n", parser->buffer);

    if (parser->buffer[0] == '\0') {
        // end of header
        parser->state = WS_HEADER;

        // read websocket frame reader from now on
        parser->remaining = 2;
        parser->parse_fn = parse_frame_header;
        parser->read_fn = read_bytes;
        return 0;
    }

    char *key = parser->buffer;
    char *value = split_header(key);
    if (value == NULL)
        return -1;

    if (!strcasecmp(key, "Sec-WebSocket-Key")) {
        if (parser->key == NULL)
            parser->key = strdup(value);
    }

    return 0;
}

// parse the http GET line
static int parse_http_get(struct ws_parser *parser)
{
    printf("parse_http_get: %s\n", parser->buffer);

    parser->parse_fn = parse_http_header;

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

    if (pos == len) {
        // no \n found, copy everything to the buffer and return
        if (concat(parser, data, pos) == -1)
            return -1;

        return len;
    }
    else {
        // \n found, copy the line to the buffer and call parse_fn

        // copy the \n char too
        pos++;

        if (concat(parser, data, pos) == -1)
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
static int read_bytes(struct ws_parser *parser, const char *data, size_t len)
{
    if (len > parser->remaining)
        len = parser->remaining;
    parser->remaining -= len;

    concat(parser, data, len);

    if (parser->remaining == 0) {
        if (parser->parse_fn(parser) == -1)
            return -1;

        parser->buffer_len = 0;
    }

    return len;
}

// read n bytes, process them in chucks as they become available
static int read_stream(struct ws_parser *parser, const char *data, size_t len)
{
    if (len > parser->remaining)
        len = parser->remaining;
    parser->remaining -= len;

    parser->state = WS_DATA;
    parser->data = data;
    parser->data_offset += parser->data_len;
    parser->data_len = len;

    // got everything, wait for next frame header
    if (parser->remaining == 0) {
        parser->remaining = 2;
        parser->read_fn = read_bytes;
        parser->parse_fn = parse_frame_header;
    }

    return len;
}

// read and parse a byte stream
// return the number of bytes consumed or -1 on error
int ws_read(struct ws_parser *parser, const char *data, size_t len)
{
    parser->state = WS_NONE;

    int consumed = 0;

    while (parser->state == WS_NONE && len > 0) {
        int ret = parser->read_fn(parser, data, len);
        if (ret == -1)
            return -1;

        consumed += ret;
        data += ret;
        len -= ret;
    }

    return consumed;
}

struct ws_parser *ws_new()
{
    struct ws_parser *parser = malloc(sizeof(struct ws_parser));

    parser->read_fn = read_line;
    parser->parse_fn = parse_http_get;

    parser->buffer = malloc(BUFFER_SIZE);
    parser->buffer_len = 0;

    parser->key = NULL;

    return parser;
}

void ws_free(struct ws_parser *parser)
{
    free(parser->key);
    free(parser->buffer);
    free(parser);
}
