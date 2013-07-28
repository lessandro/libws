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

#ifdef __linux
# include <endian.h>
# ifndef BYTE_ORDER
#  define BYTE_ORDER __BYTE_ORDER
#  define LITTLE_ENDIAN __LITTLE_ENDIAN
# endif
#endif

#include "ws.h"

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

// read n bytes, process them in chucks as they become available
static int read_stream(struct ws_parser *parser, char *data, size_t len)
{
    if (len > parser->remaining)
        len = parser->remaining;

    if (len > WS_BUFFER_SIZE - 1)
        len = WS_BUFFER_SIZE - 1;

    parser->result = WS_FRAME_CHUNK;
    parser->frame.chunk_data = data;
    parser->frame.chunk_offset = parser->frame.len - parser->remaining;
    parser->frame.chunk_len = len;

    for (int i = 0; i < len; i++)
        data[i] ^= parser->frame.mask[i % 4];

    parser->remaining -= len;
    if (parser->remaining == 0)
        ws_read_next_frame(parser);

    return len;
}

// read n bytes into the buffer and then call parse_fn
static int read_bytes(struct ws_parser *parser, char *data, size_t len)
{
    if (len > parser->remaining)
        len = parser->remaining;

    if (len > 0) {
        memcpy(parser->u.bytes + parser->bytes_len, data, len);
        parser->bytes_len += len;
        parser->remaining -= len;
    }

    if (parser->remaining == 0)
        parser->parse_fn(parser);

    return len;
}

#define read_stream_cb(parser,len,fn) \
    do { \
        parser->remaining = len; \
        parser->read_fn = read_stream; \
    } while (0)

#define read_bytes_cb(parser,len,fn) \
    do { \
        parser->remaining = len; \
        parser->bytes_len = 0; \
        parser->read_fn = read_bytes; \
        parser->parse_fn = fn; \
    } while (0)

static void parse_frame_mask(struct ws_parser *parser)
{
    for (int i = 0; i < 4; i++)
        parser->frame.mask[i] = parser->frame.masked ? parser->u.bytes[i] : 0;

    read_stream_cb(parser, parser->frame.len, ws_read_next_frame);
}

static void parse_frame_length(struct ws_parser *parser)
{
    if (parser->frame.len == 126)
        parser->frame.len = ntohs(parser->u.len16);
    else if (parser->frame.len == 127)
        parser->frame.len = ntohll(parser->u.len64);

    read_bytes_cb(parser, parser->frame.masked ? 4 : 0, parse_frame_mask);
}

static void parse_frame_header(struct ws_parser *parser)
{
    parser->frame.fin = parser->u.bytes[0] >> 7;
    parser->frame.opcode = parser->u.bytes[0] & 0x0F;
    parser->frame.len = parser->u.bytes[1] & 0x7F;
    parser->frame.masked = parser->u.bytes[1] >> 7;

    int num = 0;
    if (parser->frame.len == 126)
        num = 2;
    if (parser->frame.len == 127)
        num = 8;

    read_bytes_cb(parser, num, parse_frame_length);
}

void ws_read_next_frame(struct ws_parser *parser)
{
    read_bytes_cb(parser, 2, parse_frame_header);
}

// writes at most 10 bytes to *out
// returns the number of bytes written
// or -1 if len is greater than the maximum (2^63-1)
int ws_write_frame_header(char *out, int type, uint64_t len)
{
    // type and FIN bit
    out[0] = type | 0x80;

    // frame length (7 bits)
    if (len < 126) {
        out[1] = len;
        return 2;
    }

    void *p = &out[2];

    // frame length (16 bits)
    if (len <= 0xFFFF) {
        out[1] = 126;
        *(uint16_t *)p = htons((uint16_t)len);
        return 4;
    }

    // frame length (64 bits)
    if (len <= 0x7FFFFFFFFFFFFFFFULL) {
        out[1] = 127;
        *(uint64_t *)p = ntohll((uint64_t)len);
        return 10;
    }

    return -1;
}
