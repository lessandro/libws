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

#include <netinet/in.h>
#include "ws.h"
#include "ws_private.h"

static void parse_frame_mask(struct ws_parser *parser)
{
    for (int i = 0; i < 4; i++)
        parser->mask[i] = parser->frame_mask ? parser->buffer[i] : 0;

    read_stream_cb(parser, parser->frame_len, parse_frame_data);
}

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

static void parse_frame_length(struct ws_parser *parser)
{
    if (parser->frame_len == 126)
        parser->frame_len = ntohs(parser->len16);
    else if (parser->frame_len == 127)
        parser->frame_len = ntohll(parser->len64);

    read_bytes_cb(parser, parser->frame_mask ? 4 : 0, parse_frame_mask);
}

static void parse_frame_header(struct ws_parser *parser)
{
    parser->frame_fin = parser->header[0] >> 7;
    parser->frame_opcode = parser->header[0] & 0x0f;
    parser->frame_len = parser->header[1] & 0x7f;
    parser->frame_mask = parser->header[1] >> 7;

    int num = 0;
    if (parser->frame_len == 126)
        num = 2;
    if (parser->frame_len == 127)
        num = 8;

    read_bytes_cb(parser, num, parse_frame_length);
}

void parse_frame_data(struct ws_parser *parser)
{
    read_bytes_cb(parser, 2, parse_frame_header);
}
