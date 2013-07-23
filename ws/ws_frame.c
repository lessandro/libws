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
#include <netinet/in.h>
#include "ws.h"
#include "ws_private.h"

static uint64_t ntohll(uint64_t n)
{
#if BYTE_ORDER == LITTLE_ENDIAN
    n = ((n << 8) & 0xFF00FF00FF00FF00ULL) |
        ((n >> 8) & 0x00FF00FF00FF00FFULL);
    n = ((n << 16) & 0xFFFF0000FFFF0000ULL) |
        ((n >> 16) & 0x0000FFFF0000FFFFULL);
    n = (n << 32) | (n >> 32);
#endif
    return n;
}

static uint64_t htonll(uint64_t n)
{
    return ntohll(n);
}

// writes at most 10 bytes to *out
// returns the number of bytes written
// or -1 if len is greater than the maximum (2^63-1)
int ws_frame_header(char *out, int type, uint64_t len)
{
    out[0] = type | 0x80;

    if (len < 126) {
        out[1] = len;
        return 2;
    }

    void *p = &out[2];

    if (len <= 0xFFFF) {
        out[1] = 126;
        *(uint16_t *)p = htons((uint16_t)len);
        return 4;
    }

    if (len <= 0x7FFFFFFFFFFFFFFFULL) {
        out[1] = 127;
        *(uint64_t *)p = htonll((uint64_t)len);
        return 10;
    }

    return -1;
}

static int parse_frame_mask(struct ws_parser *parser)
{
    for (int i = 0; i < 4; i++)
        parser->frame.mask[i] = parser->frame.masked ? parser->buffer[i] : 0;

    read_stream_cb(parser, parser->frame.total_len, parse_frame_data);

    return 0;
}

static int parse_frame_length(struct ws_parser *parser)
{
    union {
        uint8_t buffer[8];
        uint16_t len16;
        uint64_t len64;
    } u;

    memcpy(u.buffer, parser->buffer, 8);

    if (parser->frame.total_len == 126)
        parser->frame.total_len = ntohs(u.len16);
    else if (parser->frame.total_len == 127)
        parser->frame.total_len = ntohll(u.len64);

    read_bytes_cb(parser, parser->frame.masked ? 4 : 0, parse_frame_mask);

    return 0;
}

static int parse_frame_header(struct ws_parser *parser)
{
    parser->frame.fin = parser->buffer[0] >> 7;
    parser->frame.opcode = parser->buffer[0] & 0x0F;
    parser->frame.len = parser->buffer[1] & 0x7F;
    parser->frame.masked = parser->buffer[1] >> 7;

    int num = 0;
    if (parser->frame.total_len == 126)
        num = 2;
    if (parser->frame.total_len == 127)
        num = 8;

    read_bytes_cb(parser, num, parse_frame_length);

    return 0;
}

int parse_frame_data(struct ws_parser *parser)
{
    parser->frame.data = parser->buffer;
    read_bytes_cb(parser, 2, parse_frame_header);

    return 0;
}
