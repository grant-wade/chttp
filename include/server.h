#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "router.h"
#include "layers.h"

typedef struct {
    String* host;
    int port;
    int server_fd;
    char* directory;
    Router* router;
    LayerCtx* layer_ctx;
    void* tag;
} HttpServer;

bool http_server_init(HttpServer* server, const char* host, int port, void* tag);
void http_server_free(HttpServer* server);
bool http_server_start(HttpServer* server);
bool http_server_stop(HttpServer* server);
void http_server_add_builtins(HttpServer* server, bool verbose);

#endif
