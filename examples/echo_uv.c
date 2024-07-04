/*
 * Copyright (c) 2024 Michael Guzov
 *
 * Licensed under the Apache License Version 2.0 with LLVM Exceptions
 * (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 *   https://llvm.org/LICENSE.txt
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <uv.h>

#include <stdlib.h>
#include <stdbool.h>

enum {
    READABLE_BUFFER_SIZE = 65536
};

///////////////////////////////////////////////////////////////////////////////
/// Server
///////////////////////////////////////////////////////////////////////////////

static char* SERVER_READ_BUFFER = NULL;

static int CLIENT_COUNT = 0;

typedef struct SServerData {
    int client_id;
} TServerData;

typedef struct SReqData {
    uv_buf_t buf;
    uv_stream_t* stream;
} TReqData;

static void on_server_close(uv_handle_t* handle) {
    free(handle->data);
    free(handle);
}

static void on_server_shutdown(uv_shutdown_t* req, int status) {
    uv_stream_t* stream = req->data;
    free(req);
    uv_close((uv_handle_t*)stream, on_server_close);
    if (status < 0) {
        fprintf(stderr, "Server: Unable to shutdown TCP connection -> %s\n", uv_strerror(status));
    }
}

static void on_server_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    buf->base = SERVER_READ_BUFFER;
    buf->len = READABLE_BUFFER_SIZE;
}

static void on_server_write(uv_write_t* req, int status) {
    // if we got here then our write completed either we got an error
    // in any case we need to delete allocated data from memory
    TReqData* req_data = req->data;
    uv_stream_t* stream = req_data->stream;
    free(req_data->buf.base);
    free(req_data);
    free(req);
    if (status < 0) {
        uv_close((uv_handle_t*)stream, on_server_close);
        fprintf(stderr, "Server: Unable to write to TCP connection -> %s\n", uv_strerror(status));
        return;
    }
}

static void on_server_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    if (nread < 0) {
        uv_read_stop(stream);

        if (nread != UV_EOF) {
            uv_close((uv_handle_t*)stream, on_server_close);
            fprintf(stderr, "Server: Connection Reading Error -> %s\n", uv_strerror((int) nread));
            return;
        }

        uv_shutdown_t* shutdown_handle = malloc(sizeof(uv_shutdown_t));
        shutdown_handle->data = stream;
        nread = uv_shutdown(shutdown_handle, stream, on_server_shutdown);
        if (nread < 0) {
            free(shutdown_handle);
            uv_close((uv_handle_t*)stream, on_server_close);
            fprintf(stderr, "Server: Unable to shutdown -> %s\n", uv_strerror((int) nread));
        }
        return;
    }

    TReqData* req_data = malloc(sizeof(TReqData));
    req_data->stream = stream;
    req_data->buf.base = SERVER_READ_BUFFER;
    req_data->buf.len = (size_t) nread;
    SERVER_READ_BUFFER = malloc(READABLE_BUFFER_SIZE * sizeof(char));

    uv_write_t* write_handle = malloc(sizeof(uv_write_t));
    write_handle->data = req_data;
    nread = uv_write(write_handle, stream, &req_data->buf, 1, on_server_write);
    if (nread < 0) {
        free(req_data->buf.base);
        free(req_data);
        free(write_handle);
        uv_close((uv_handle_t*)stream, on_server_close);
        fprintf(stderr, "Server: Connection Writing Error -> %s\n", uv_strerror((int) nread));
    }
}

static void on_connection(uv_stream_t* server, int status) {
    if (status < 0) {
        fprintf(stderr, "Server: Unable to accept TCP connection -> %s\n", uv_strerror(status));
        return;
    }

    uv_tcp_t* client = malloc(sizeof(uv_tcp_t));
    uv_tcp_init(server->loop, client);

    TServerData* server_data = malloc(sizeof(TServerData));
    client->data = server_data;

    status = uv_accept(server, (uv_stream_t*) client);
    if (status < 0) {
        uv_close((uv_handle_t*)client, on_server_close);
        fprintf(stderr, "Server: Unable to accept TCP connection -> %s\n", uv_strerror(status));
        return;
    }

    server_data->client_id = ++CLIENT_COUNT;

    status = uv_read_start((uv_stream_t*)client, on_server_alloc, on_server_read);
    if (status < 0) {
        uv_close((uv_handle_t*)client, on_server_close);
        fprintf(stderr, "Server: Unable to start reading from TCP connection -> %s\n", uv_strerror(status));
    }
}

static uv_async_t STOP_TOKEN;

static void stop_callback(uv_async_t* stop_token) {
    uv_close((uv_handle_t*) stop_token->data, NULL);
    uv_close((uv_handle_t*) stop_token, NULL);
}

void UvEchoServer(int port) {
    // making LibUV loop
    uv_loop_t loop;
    uv_loop_init(&loop);
    // making tcp server handle
    uv_tcp_t tcp_server;
    uv_tcp_init(&loop, &tcp_server);

    uv_async_init(&loop, &STOP_TOKEN, stop_callback);
    STOP_TOKEN.data = &tcp_server;
    SERVER_READ_BUFFER = malloc(READABLE_BUFFER_SIZE * sizeof(char));

    // resolving address to bind TCP server
    struct sockaddr_in addr;
    int err = uv_ip4_addr("127.0.0.1", port, &addr);
    if (err < 0) {
        fprintf(stderr, "Server: Unable to Resolve given address on port %d -> %s\n", port, uv_strerror(err));
        return;
    }

    // binding TCP server
    err = uv_tcp_bind(&tcp_server, (const struct sockaddr*) &addr, 0);
    if (err < 0) {
        fprintf(stderr, "Server: Unable to bind TCP server on port %d -> %s\n", port, uv_strerror(err));
        return;
    }

    // listening TCP server on given port
    err = uv_listen((uv_stream_t*) &tcp_server, 128, on_connection);
    if (err < 0) {
        fprintf(stderr, "Server: Unable to Listen TCP server on port %d -> %s\n", port, uv_strerror(err));
        return;
    }

    // starting loop
    uv_run(&loop, UV_RUN_DEFAULT);

    free(SERVER_READ_BUFFER);
    fflush(stderr);
}

void UvEchoServerStop(void) {
    uv_async_send(&STOP_TOKEN);
}

///////////////////////////////////////////////////////////////////////////////
/// Client
///////////////////////////////////////////////////////////////////////////////

static char CLIENT_READ_BUFFER[READABLE_BUFFER_SIZE];

typedef struct SClientStats {
    int connection_limit;
    int processed_connections;
    int fd_limit_exceeded;
    long long total_bytes_received;
} TClientStats;

typedef struct SClientData {
    struct sockaddr_in* addr;
    const uv_buf_t* data_buffer;
    ssize_t bytes_left;
    int rc;
} TClientData;

static int new_connection(uv_loop_t* loop, struct sockaddr_in* addr, const uv_buf_t* buf);

static void on_client_close(uv_handle_t* handle) {
    TClientStats* client_stats = handle->loop->data;
    TClientData* client_data = handle->data;
    ++client_stats->processed_connections;
    if (client_stats->fd_limit_exceeded > 0 && --client_stats->connection_limit > 0) {
        int status = new_connection(
                handle->loop, client_data->addr, client_data->data_buffer);
        if (status < 0) {
            fprintf(stderr, "Client: Unable to connect to TCP server -> %s\n", uv_strerror(status));
            if (status == UV_EMFILE) {
                ++client_stats->fd_limit_exceeded;
            }
        }
    }
    free(handle->data);
    free(handle);
}

static void on_client_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    buf->base = CLIENT_READ_BUFFER;
    buf->len = READABLE_BUFFER_SIZE;
}

static void on_connect(uv_connect_t* req, int status);

int new_connection(uv_loop_t* loop, struct sockaddr_in* addr, const uv_buf_t* buf) {
    TClientData* client_data = malloc(sizeof(TClientData));
    client_data->data_buffer = buf;
    client_data->bytes_left = (ssize_t) buf->len;
    client_data->addr = addr;
    client_data->rc = 2;

    uv_tcp_t* client = malloc(sizeof(uv_tcp_t));
    uv_tcp_init(loop, client);
    uv_connect_t* conn_req = malloc(sizeof(uv_connect_t));

    client->data = client_data;
    conn_req->data = client;
    return uv_tcp_connect(conn_req, client, (struct sockaddr*) addr, on_connect);
}

static void on_client_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    TClientData* client_data = stream->data;
    if (nread < 0) {
        uv_read_stop(stream);
        if (--client_data->rc == 0) {
            uv_close((uv_handle_t*) stream, on_client_close);
        }
        fprintf(stderr, "Client: Unable to read from TCP connection -> %s\n", uv_strerror((int) nread));
        return;
    }
    TClientStats* client_stats = stream->loop->data;

    if (client_stats->fd_limit_exceeded == 0) {
        bool first_read = client_data->bytes_left == (ssize_t) client_data->data_buffer->len;
        if (first_read && --client_stats->connection_limit > 0) {
            int status = new_connection(stream->loop, client_data->addr, client_data->data_buffer);
            if (status < 0) {
                fprintf(stderr, "Client: Unable to connect to TCP server -> %s\n", uv_strerror(status));
                if (status == UV_EMFILE) {
                    ++client_stats->fd_limit_exceeded;
                }
            }
        }
    }

    TClientStats* stats = stream->loop->data;
    stats->total_bytes_received += nread;

    client_data->bytes_left -= nread;
    if (client_data->bytes_left <= 0) {
        uv_read_stop(stream);
        if (--client_data->rc == 0) {
            uv_close((uv_handle_t*) stream, on_client_close);
        }
    }
}

static void on_client_write(uv_write_t* req, int status) {
    uv_stream_t* stream = req->data;
    free(req);
    TClientData* client_data = stream->data;
    if (status < 0) {
        fprintf(stderr, "Client: Unable to write to TCP server: %s\n", uv_strerror(status));
    }
    if (--client_data->rc == 0) {
        uv_close((uv_handle_t*) stream, on_client_close);
    }
}

void on_connect(uv_connect_t* req, int status) {
    uv_stream_t* stream = req->data;
    free(req);
    if (status < 0) {
        uv_close((uv_handle_t*) stream, on_client_close);
        fprintf(stderr, "Client: Unable to connect to TCP server -> %s\n", uv_strerror(status));
        return;
    }

    TClientData* client_data = stream->data;

    status = uv_read_start(stream, on_client_alloc, on_client_read);
    if (status < 0) {
        uv_close((uv_handle_t*) stream, on_client_close);
        fprintf(stderr, "Client: Unable to start reading from TCP connection -> %s\n", uv_strerror(status));
        return;
    }

    uv_write_t* write_handle = malloc(sizeof(uv_write_t));
    write_handle->data = stream;
    status = uv_write(write_handle, stream, client_data->data_buffer, 1, on_client_write);
    if (status < 0) {
        uv_read_stop(stream);
        free(write_handle);
        uv_close((uv_handle_t*) stream, on_client_close);
        fprintf(stderr, "Client: Unable to write to TCP server -> %s\n", uv_strerror(status));
    }
}

long long UvEchoClient(int port, int connections, int init_conn, const char* data, size_t data_len) {
    TClientStats stats = {
            .connection_limit = connections - init_conn + 1,
            .processed_connections = 0,
            .fd_limit_exceeded = 0,
            .total_bytes_received = 0
    };
    // making LibUV loop
    uv_loop_t loop;
    uv_loop_init(&loop);
    loop.data = &stats;

    // resolving address to connect to TCP server
    struct sockaddr_in addr;
    int err = uv_ip4_addr("127.0.0.1", port, &addr);
    if (err < 0) {
        fprintf(stderr, "Unable to Resolve given address on port %d -> %s\n", port, uv_strerror(err));
        return 0;
    }

    uv_buf_t buf = { .base = (char*) data, .len = data_len };
    for (int i = 0; i < init_conn; ++i) {
        new_connection(&loop, &addr, &buf);
    }

    // starting loop
    uv_run(&loop, UV_RUN_DEFAULT);
    return stats.total_bytes_received;
}
