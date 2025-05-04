#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include "alloc.h"
#include "builtin.h"
#include "routes.h"
#include "server.h"

typedef struct {
    int client_fd;
    HttpServer* server;
} InternalRequest;

void* handle_client_request(void* ctx) {
    InternalRequest* request = ctx;
    HttpServer* server = request->server;
    int client_fd = request->client_fd;
    void* tag = TAG(client_fd);
    bool active = true;

    while (active) {

        // get the client's request
        char buffer[4096];
        ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            if (bytes_received < 0) {
                printf("Receive failed: %s \n", strerror(errno));
            } else {
                printf("Client closed connection\n");
            }
            break;
        }

        // start time
        struct timespec start;
        clock_gettime(CLOCK_REALTIME, &start);

        buffer[bytes_received] = '\0'; // Null-terminate the received data

        // parse the request
        HttpRequest* request = http_request_new(tag);
        if (!request) {
            printf("Failed to allocate memory for HttpRequest\n");
            break;
        }

        // turn the request into a string
        String* request_string = string_new(buffer, tag);

        // parse the request string
        bool status = http_request_parse(request, request_string);

        if (!status) {
            printf("Failed to parse HTTP request\n");
            layers_apply(server->layer_ctx, LAYER_CLEANUP, request, NULL);
            http_request_free(request);
            break;
        }

        HttpResponse* response = http_response_new(tag);

        layers_apply(server->layer_ctx, LAYER_PRE_ROUTE, request, response);
        router_route(server->router, request, response);
        layers_apply(server->layer_ctx, LAYER_POST_ROUTE, request, response);

        if (!http_response_send(response, client_fd)) {
            printf("Failed to send HTTP response\n");
            layers_apply(server->layer_ctx, LAYER_CLEANUP, request, response);
            http_request_free(request);
            http_response_free(response);
            break;
        }

        // end time
        struct timespec end;
        clock_gettime(CLOCK_REALTIME, &end);

        // calculate the time taken in microseconds
        long time_taken = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
        printf("TIME: %ld microseconds\n", time_taken);

        // check for Connection: close header
        Header* connection_header = http_request_get_header(request, "Connection");
        if (connection_header && string_equals_cstr(connection_header->value, "close")) {
            printf("Connection: close header found, closing connection\n");
            active = false;
        } else {
            printf("Connection: keep-alive header found or not present, keeping connection open\n");
        }

        layers_apply(server->layer_ctx, LAYER_CLEANUP, request, response);
        http_request_free(request);
        http_response_free(response);
        pfree_tag(tag);
    }

    if (close(client_fd) < 0) {
        printf("Close failed: %s \n", strerror(errno));
    }

    pfree(request);
    return NULL;
}

bool http_server_init(HttpServer* server, const char* host, int port, void* tag) {
    if (!server || !host || port <= 0) return false;

    server->host = string_new(host, tag);
    server->port = port;
    server->tag = tag;

    server->router = router_new(tag);
    if (!server->router) {
        string_free(server->host);
        return false;
    }

    server->layer_ctx = layers_new(tag);
    if (!server->layer_ctx) {
        router_free(server->router);
        string_free(server->host);
        return false;
    }

    return true;
}

void http_server_free(HttpServer* server) {
    if (!server) return;

    router_free(server->router);
    layers_free(server->layer_ctx);
    string_free(server->host);
}


bool http_server_stop(HttpServer* server);

bool http_server_start(HttpServer* server) {
    int server_fd, client_addr_len;
	struct sockaddr_in client_addr;

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}

	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		printf("SO_REUSEADDR failed: %s \n", strerror(errno));
		return 1;
	}

	// convert from string host to IP address
	struct in_addr addr;
	if (inet_pton(AF_INET, string_cstr(server->host), &addr) <= 0) {
        printf("Invalid address: %s \n", string_cstr(server->host));
        return 1;
    }

	struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
									 .sin_port = htons(server->port),
									 .sin_addr = addr,
									};

	if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}


	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}
	printf("Server started on %s:%d\n", string_cstr(server->host), server->port);
	printf("Waiting for a client to connect...\n");

	// oh look a loop
    while (1) {
        // Accept a new client connection
        int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t*)&client_addr_len);
        printf("Client connected\n");

        if (client_fd < 0) {
            printf("Accept failed: %s \n", strerror(errno));
            return 1;
        }

        // create the InternalRequest struct
        InternalRequest* request = pmalloc(sizeof(InternalRequest), server->tag);
        if (!request) {
            printf("Failed to allocate memory for InternalRequest\n");
            close(client_fd);
            return 1;
        }
        request->client_fd = client_fd;
        request->server = server;

        // spawn a thread that calls handle_client_request
        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client_request, request) != 0) {
            printf("Thread creation failed: %s \n", strerror(errno));
            close(client_fd);
            return 1;
        }

        // Detach the thread to allow it to run independently
        if (pthread_detach(thread) != 0) {
            printf("Thread detach failed: %s \n", strerror(errno));
            close(client_fd);
            return 1;
        }
    }

    close(server_fd);
}

void http_server_add_builtins(HttpServer* server, bool verbose) {
    // add built-in routes
    router_add_route(server->router, "/", HTTP_GET, index_route, true);
	router_add_route(server->router, "/echo", HTTP_GET, echo_route, false);
	router_add_route(server->router, "/user-agent", HTTP_GET, user_agent_route, false);

	// add built-in layers
	if (verbose) {
    	layers_add(server->layer_ctx, LAYER_PRE_ROUTE, "logging-preroute", logging_layer_preroute_verbose, true);
    	layers_add(server->layer_ctx, LAYER_POST_ROUTE, "logging-postroute", logging_layer_postroute_verbose, true);
	} else {
    	layers_add(server->layer_ctx, LAYER_PRE_ROUTE, "logging-preroute", logging_layer_preroute_basic, true);
       	layers_add(server->layer_ctx, LAYER_POST_ROUTE, "logging-postroute", logging_layer_postroute_basic, true);
	}
    layers_add(server->layer_ctx, LAYER_POST_ROUTE, "content-encoding", content_encoding_layer, true);
	layers_add(server->layer_ctx, LAYER_POST_ROUTE, "content-length", content_length_layer, true);
	layers_add(server->layer_ctx, LAYER_POST_ROUTE, "connection-close", connection_close_layer, true);
	layers_add(server->layer_ctx, LAYER_POST_ROUTE, "request-memory-usage", request_memory_usage_layer, true);
}
