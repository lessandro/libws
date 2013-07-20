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
#include <string.h>
#include "ws.h"
#include "ws_private.h"
#include "sha1.h"
#include "base64.h"

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
    sha1_result(&ctx, result);

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

    strcpy((char *)parser->buffer, http_reply);
    strcat((char *)parser->buffer, encoded);
    strcat((char *)parser->buffer, "\r\n\r\n");
}

// split an http header line ("key: value")
// return a pointer to the value
// zero-length keys and values are allowed
static char *split_header(char *line)
{
    char *value = strchr(line, ':');
    if (value == NULL)
        return line + strlen(line);

    // null-terminate key
    *value++ = '\0';

    // remove leading spaces
    while (*value == ' ')
        value++;

    return value;
}

// parse one http header line
static void parse_http_header(struct ws_parser *parser)
{
    printf("parse_http_header: %s\n", parser->buffer);

    if (parser->buffer[0] == '\0') {
        // end of header
        parser->result = WS_HEADER;

        // read websocket frame reader from now on
        parse_frame_data(parser);
        return;
    }

    char *key = (char *)parser->buffer;
    char *value = split_header(key);

    if (!strcasecmp(key, "Sec-WebSocket-Key")) {
        if (parser->key == NULL)
            parser->key = strdup(value);
    }

    read_line_cb(parser, parse_http_header);
}

// parse the http GET line
void parse_http_get(struct ws_parser *parser)
{
    printf("parse_http_get: %s\n", parser->buffer);

    read_line_cb(parser, parse_http_header);
}
