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

static void send_error(struct sev_stream *stream)
{
    char buffer[WS_HTTP_RESPONSE_SIZE];
    ws_write_http_error(buffer);
    sev_send(stream, buffer, strlen(buffer));
}

static int header_cb(struct ws_header *header, void *data)
{
    printf("resource: %s\n", header->resource);

    char buffer[WS_HTTP_RESPONSE_SIZE];
    ws_write_http_handshake(buffer, header->websocket_key);
    sev_send(data, buffer, strlen(buffer));

    return 0;
}

static int frame_cb(struct ws_frame *frame, void *data)
{
    printf("got %ld bytes @ offset %lld / frame len %lld / opcode %d\n",
        frame->chunk_len, frame->chunk_offset,
        frame->len, frame->opcode);

    frame->chunk_data[frame->chunk_len] = '\0';
    printf("%s\n", frame->chunk_data);

    // send the data back to the client
    char header[10];
    int header_len = ws_write_frame_header(header, WS_TEXT, frame->chunk_len);
    sev_send(data, header, header_len);
    sev_send(data, frame->chunk_data, frame->chunk_len);

    return 0;
}

static void open_cb(struct sev_stream *stream)
{
    printf("open %s:%d\n", stream->remote_address, stream->remote_port);

    struct ws_parser *parser = malloc(sizeof(struct ws_parser));
    ws_parser_init(parser);
    parser->header_cb = header_cb;
    parser->frame_cb = frame_cb;

    parser->data = stream;
    stream->data = parser;
}

static void read_cb(struct sev_stream *stream, const char *data, size_t len)
{
    // cast the const away
    if (ws_parse_all(stream->data, (char *)data, len) == -1)
        send_error(stream);
}

static void close_cb(struct sev_stream *stream)
{
    printf("close %s\n", stream->remote_address);
    ws_parser_free(stream->data);
    free(stream->data);
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
