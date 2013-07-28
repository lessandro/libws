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
#include <strings.h>
#include "ws.h"
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

void ws_write_http_handshake(char *out, char *key)
{
    char encoded[base64_encode_len(SHA1_RESULTLEN)];

    compute_challenge(key, encoded);

    strcpy(out, http_reply);
    strcat(out, encoded);
    strcat(out, "\r\n\r\n");
}

static const char *http_error =
    "HTTP/1.1 400 Bad Request\r\n"
    "Sec-WebSocket-Version: 13\r\n"
    "\r\n";

void ws_write_http_error(char *out)
{
    strcpy(out, http_error);
}

static char *split(char *line, const char *delim)
{
    if (!line)
        return NULL;

    char *str = strstr(line, delim);
    if (!str)
        return NULL;

    // remove delimiter
    while (*delim++)
        *str++ = '\0';

    // remove leading whitespace
    while (*str == ' ')
        str++;

    return str;
}

int parse_http_header(struct ws_parser *parser)
{
    char *next = split(parser->buffer, "\r\n");

    char *method = parser->buffer;
    char *resource = split(method, " ");
    char *http = split(resource, " ");

    if (!http || strcmp(method, "GET") || strcmp(http, "HTTP/1.1"))
        return -1;

    parser->header.resource = resource;

    int num_headers = 0;
    for (char *p = next; *p; p++)
        num_headers += (*p == '\n');

    parser->header.headers = calloc(num_headers + 1, sizeof(char *));
    parser->header.values = calloc(num_headers + 1, sizeof(char *));
    num_headers = 0;

    int upgrade = 0;
    int connection = 0;
    int version = 0;

    while (strncmp(next, "\r\n", 2)) {
        char *header = next;
        char *value = split(header, ":");
        next = split(value, "\r\n");

        if (!next)
            return -1;

        parser->header.headers[num_headers] = header;
        parser->header.values[num_headers++] = value;

        if (!strcasecmp(header, "Upgrade")) {
            if (strcasecmp(value, "websocket"))
                return -1;
            upgrade = 1;
        }
        else if (!strcasecmp(header, "Connection")) {
            if (strcasecmp(value, "Upgrade"))
                return -1;
            connection = 1;
        }
        else if (!strcasecmp(header, "Sec-WebSocket-Version")) {
            if (strcasecmp(value, "13"))
                return -1;
            version = 1;
        }
        else if (!strcasecmp(header, "Sec-WebSocket-Key")) {
            parser->header.websocket_key = value;
            continue;
        }
    }

    if (!upgrade || !connection || !version || !parser->header.websocket_key)
        return -1;

    parser->result = WS_HTTP_HEADER;
    ws_read_next_frame(parser);

    return 0;
}

// read the http header into the buffer and then call parse_http_header
int ws_read_http_header(struct ws_parser *parser, char *data, size_t len)
{
    int pos = 0;

    for (; pos < len; pos++) {
        if (parser->buffer_len == WS_BUFFER_SIZE - 1) {
            parser->errno = WS_BUFFER_OVERFLOW;
            return -1;
        }

        parser->buffer[parser->buffer_len++] = data[pos];

        if (parser->buffer_len < 4)
            continue;

        char *p = parser->buffer + parser->buffer_len - 4;
        if (strncmp(p, "\r\n\r\n", 4) == 0) {
            parser->buffer[parser->buffer_len] = '\0';

            if (parse_http_header(parser) == -1) {
                parser->errno = WS_BAD_REQUEST;
                return -1;
            }

            break;
        }
    }

    return pos + 1;
}
