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

#include "ws.h"
#include "ws_private.h"

// read and parse a byte stream
// return the number of bytes read
int ws_parse(struct ws_parser *parser, const char *data, size_t len)
{
    parser->result = WS_NONE;
    return parser->read_fn(parser, data, len);
}

int ws_parse_all(struct ws_parser *parser, const char *data, size_t len)
{
    while (len > 0) {
        int ret = ws_parse(parser, data, len);

        if (parser->result == WS_HEADER && parser->header_cb)
            parser->header_cb(parser);

        if (parser->result == WS_FRAME && parser->frame_cb)
            parser->frame_cb(parser);

        data += ret;
        len -= ret;
    }

    return 0;
}

struct ws_parser *ws_parser_new()
{
    struct ws_parser *parser = calloc(1, sizeof(struct ws_parser));
    read_line_cb(parser, parse_http_get);
    return parser;
}

void ws_parser_free(struct ws_parser *parser)
{
    free(parser->key);
    free(parser);
}
