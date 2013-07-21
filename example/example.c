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

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <ev.h>
#include "sev/sev.h"
#include "../ws/ws.h"

#define PORT 8888

static void header_cb(struct ws_parser *parser)
{
    struct sev_stream *stream = parser->data;

    ws_http_reply(parser);
    sev_send(stream, parser->buffer, strlen(parser->buffer));
}

static void frame_cb(struct ws_parser *parser)
{
    printf("got %ld bytes @ offset %lld / frame len %lld / opcode %d\n",
        parser->chunk_len, parser->chunk_offset,
        parser->frame_len, parser->frame_opcode);

    parser->buffer[parser->chunk_len] = '\0';
    printf("%s\n", parser->buffer);
}

static void open_cb(struct sev_stream *stream)
{
    printf("open %s:%d\n", stream->remote_address, stream->remote_port);

    struct ws_parser *parser = ws_parser_new();
    parser->header_cb = header_cb;
    parser->frame_cb = frame_cb;

    parser->data = stream;
    stream->data = parser;
}

static void read_cb(struct sev_stream *stream, const char *data, size_t len)
{
    struct ws_parser *parser = stream->data;

    ws_parse_all(parser, data, len);
}

static void close_cb(struct sev_stream *stream)
{
    printf("close %s\n", stream->remote_address);
    ws_parser_free(stream->data);
}

int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN);

    struct sev_server server;

    if (sev_listen(&server, PORT)) {
        perror("sev_listen");
        return -1;
    }

    server.open_cb = open_cb;
    server.read_cb = read_cb;
    server.close_cb = close_cb;

    ev_loop(EV_DEFAULT_ 0);

    return 0;
}
