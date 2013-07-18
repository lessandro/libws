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
#include "ws.h"

#define PORT 8888

void open_cb(struct sev_stream *stream)
{
    printf("open %s:%d\n", stream->remote_address, stream->remote_port);

    stream->data = ws_new();
}

void read_cb(struct sev_stream *stream, const char *data, size_t len)
{
    while (len > 0) {
        int ret = ws_read(stream->data, data, len);

        if (ret == -1) {
            printf("error\n");
            return;
        }

        data += ret;
        len -= ret;
    }
}

void close_cb(struct sev_stream *stream)
{
    printf("close %s\n", stream->remote_address);
    ws_free(stream->data);
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