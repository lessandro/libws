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

#ifndef WS_H
#define WS_H

#include <stdlib.h>
#include <stdint.h>

#define WS_NONE 0
#define WS_HTTP_HEADER 1
#define WS_FRAME_CHUNK 2

// frame types
#define WS_CONTINUATION 0x0
#define WS_TEXT 0x1
#define WS_BINARY 0x2
#define WS_CONNECTION_CLOSE 0x8
#define WS_PING 0x9
#define WS_PONG 0xA

// error types
#define WS_ERROR -1
#define WS_BUFFER_OVERFLOW 1
#define WS_BAD_REQUEST 2

// http header lines must fit in this buffer
#define WS_BUFFER_SIZE 4096

#define WS_FRAME_HEADER_SIZE 10
#define WS_HTTP_RESPONSE_SIZE 130

struct ws_parser;
typedef int (ws_callback)(struct ws_parser*);

struct ws_frame {
    int fin;
    int opcode;
    int masked;
    char mask[4];
    uint64_t len;

    char *chunk_data;
    size_t chunk_len;
    uint64_t chunk_offset;
};

struct ws_header {
    char *resource;
    char *websocket_key;
    char **headers;
    char **values;
};

struct ws_parser {
    int result;
    int errno;

    // parser internal state
    uint64_t remaining;
    int (*read_fn)(struct ws_parser *, char *, size_t);
    void (*parse_fn)(struct ws_parser *);

    // big buffer for http headers
    char buffer[WS_BUFFER_SIZE];
    size_t buffer_len;

    // small buffer for frame headers
    union {
        uint8_t bytes[8];
        uint16_t len16;
        uint64_t len64;
    } u;
    size_t bytes_len;

    // callbacks
    int (*header_cb)(struct ws_header *header, void *data);
    int (*frame_cb)(struct ws_frame *frame, void *data);

    // private data for the callbacks
    void *data;

    struct ws_header header;
    struct ws_frame frame;
};

int ws_write_frame_header(char *out, int type, uint64_t len);
void ws_write_http_handshake(char *out, char *key);
void ws_write_http_error(char *out);

int ws_parse_all(struct ws_parser *parser, char *data, size_t len);
int ws_parse(struct ws_parser *parser, char *data, size_t len);

void ws_parser_init(struct ws_parser *);
void ws_parser_free(struct ws_parser *);

int ws_read_http_header(struct ws_parser *parser, char *data, size_t len);
void ws_read_next_frame(struct ws_parser *);

#endif
