#include "server.h"
#define CLI_IMPLEMENTATION

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include "cli.h"
#include "cstring.h"
#include "alloc.h"
#include "http.h"
#include "router.h"
#include "routes.h"

// setup a handler for ctrl+c
void sigint_handler(int signum) {
    (void)signum;
    printf("Caught SIGINT, exiting...\n");
    pallocator_cleanup();
    exit(0);
}

void hello_handler(HttpRequest* req, HttpResponse* res) {
    res->status = HTTP_200;
    res->body = string_new("Hello, World!", req->tag);
}

int main(int argc, char** argv) {
    bool verbose;
    int port;
    const char* directory = NULL;

    CLI_BEGIN(options, argc, argv)
        CLI_FLAG('v', "verbose", verbose, "Enable verbose output")
        CLI_INT('p', "port", port, 8080, "Port number (default: 8080)")
        CLI_STRING('d', "directory", directory, NULL, "Path to search for files")
    CLI_END(options);

    // Register signal handler for SIGINT
    signal(SIGINT, sigint_handler);

    HttpServer server;
    void* tag = TAG(&server);
    http_server_init(&server, "0.0.0.0", port, tag);

    // builtin files routes isn't enabled by default let's add it
	set_file_search_dir(string_new(directory, tag));
	router_add_route(server.router, "/files", HTTP_GET | HTTP_POST, files_route, false);
	router_add_route(server.router, "/hello", HTTP_GET, hello_handler, false);

	// add built-in routes
    http_server_add_builtins(&server, verbose);

	// start the server (blocking)
	if (http_server_start(&server) != 0) {
	    printf("Failed to start server: %s\n", strerror(errno));
	    http_server_free(&server);
	}

	pfree_tag(tag);

    // some memory leak checking
    if (verbose) palloc_print_state();
    pallocator_cleanup();
    return 0;
}
