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
#define WS_HEADER 1
#define WS_FRAME 2

// must be at least 130 bytes long (see ws_http_reply)
// http header lines must fit in this buffer
#define WS_BUFFER_SIZE 4096

struct ws_parser;
typedef void (ws_callback)(struct ws_parser *);

struct ws_parser {
    // WS_NONE / WS_HEADER / WS_FRAME
    int result;

    // parser internal state
    uint64_t remaining;
    int (*read_fn)(struct ws_parser *, const char *, size_t);
    ws_callback *parse_fn;

    union {
        char buffer[WS_BUFFER_SIZE];
        uint8_t header[2];
        uint16_t len16;
        uint64_t len64;
    };
    size_t buffer_len;
    int buffer_overflow;

    // callbacks
    ws_callback *header_cb;
    ws_callback *frame_cb;

    // unused
    void *data;

    // http info
    char *key;

    // frame info
    size_t chunk_len;
    uint64_t chunk_offset;
    uint64_t frame_len;
    int frame_fin;
    int frame_opcode;
    int frame_mask;
    char mask[4];
};

void ws_http_reply(struct ws_parser *parser);

int ws_parse_all(struct ws_parser *parser, const char *data, size_t len);
int ws_parse(struct ws_parser *parser, const char *data, size_t len);

struct ws_parser *ws_new();
void ws_free(struct ws_parser *parser);

#endif
