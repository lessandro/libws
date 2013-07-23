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
#define WS_GET 1
#define WS_HEADER 2
#define WS_FRAME 3

// frame types
#define WS_CONTINUATION 0x0
#define WS_TEXT 0x1
#define WS_BINARY 0x2
#define WS_CONNECTION_CLOSE 0x8
#define WS_PING 0x9
#define WS_PONG 0xA

// error types
#define WS_BUFFER_OVERFLOW 1
#define WS_INVALID_HTTP_GET 2
#define WS_INVALID_HTTP_HEADER 3

// http header lines must fit in this buffer
#define WS_BUFFER_SIZE 4096

#define WS_MAX_FRAME_HEADER_SIZE 10
#define WS_HTTP_HANDSHAKE_SIZE 130

struct ws_parser;
typedef int (ws_callback)(struct ws_parser*);

struct ws_frame {
    int fin;
    int opcode;
    int masked;
    char mask[4];
    uint64_t total_len;

    char *data;
    size_t len;
    uint64_t offset;
};

struct ws_parser {
    // WS_NONE / WS_HEADER / WS_FRAME
    int result;
    char *key;
    char *value;
    int errno;

    // parser internal state
    uint64_t remaining;
    int (*read_fn)(struct ws_parser *, const char *, size_t);
    int (*parse_fn)(struct ws_parser *);

    char buffer[WS_BUFFER_SIZE];
    size_t buffer_len;

    // callbacks
    int (*get_cb)(struct ws_parser *, char *path);
    int (*header_cb)(struct ws_parser *, char *key, char *value);
    int (*frame_cb)(struct ws_parser *, struct ws_frame *frame);

    // private data for the callbacks
    void *data;

    struct ws_frame frame;
};

int ws_write_frame_header(char *out, int type, uint64_t len);
void ws_write_http_handshake(char *out, char *key);
void ws_write_http_error(char *out);

int ws_parse_all(struct ws_parser *parser, const char *data, size_t len);
int ws_parse(struct ws_parser *parser, const char *data, size_t len);

struct ws_parser *ws_parser_new();
void ws_parser_free(struct ws_parser *parser);

#endif
