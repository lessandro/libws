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

// read and parse a byte stream
// return the number of bytes read
int ws_parse(struct ws_parser *parser, char *data, size_t len)
{
    parser->result = WS_NONE;
    return parser->read_fn(parser, data, len);
}

int ws_parse_all(struct ws_parser *parser, char *data, size_t len)
{
    while (len > 0) {
        int ret = ws_parse(parser, data, len);
        if (ret == -1)
            return -1;

        if (parser->result == WS_HTTP_HEADER && parser->header_cb)
            if (parser->header_cb(&parser->header, parser->data) == -1)
                return -1;

        if (parser->result == WS_FRAME_CHUNK && parser->frame_cb)
            if (parser->frame_cb(&parser->frame, parser->data) == -1)
                return -1;

        data += ret;
        len -= ret;
    }

    return 0;
}

void ws_parser_init(struct ws_parser *parser)
{
    memset(parser, 0, sizeof(struct ws_parser));
    parser->read_fn = ws_read_http_header;
}

void ws_parser_free(struct ws_parser *parser)
{
    free(parser->header.headers);
    parser->header.headers = NULL;

    free(parser->header.values);
    parser->header.values = NULL;
}
