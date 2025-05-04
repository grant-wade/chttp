# CHTTP

**Author:** Grant Wade <<grant@wade.it.com>>

## Description

**CHTTP** is a simple, modular HTTP server written in C. It features a modern, extensible API with routing, lifecycle-based middleware (layers), persistent connections, and a custom memory allocator for debugging and leak detection. The design is focused on clarity, hackability, and portability, not on being a full-featured web server.

This project was built while following the [CodeCrafters Build Your Own HTTP Server course](https://app.codecrafters.io/courses/http-server).

## Features

- 🚦 **Simple Routing**: Register routes with method/path matching.
- 🧩 **Middleware (Layers)**: Add layers at any request/response lifecycle stage.
- 🔄 **Persistent Connections**: HTTP/1.1 keep-alive support.
- ⚠️ **Error Handling**: Handles common HTTP errors.
- 📝 **Logging**: Built-in and customizable logging layers.
- 📦 **Request & Response Handling**: Parse and build HTTP messages.
- 🌐 **HTTP/1.1 Support**: Handles most HTTP/1.1 requests.
- 🗂️ **Static File Serving**: Serve files from a directory.
- 🧪 **Memory Debugging**: Custom allocator with tagging and leak detection.
- 🛠️ **Cross-Platform Build**: Minimal, portable build system (`cbuild.h`).
- 🧰 **Modular Server API**: Compose your own server with routers, layers, and builtins.
- 🖥️ **Command-Line Interface**: Modern CLI parsing with help, defaults, and type safety.

## Getting Started

### Prerequisites

- **Linux** or **macOS** (Windows may work with minor tweaks)
- `gcc` or `clang` (Modern C23 was used, check build.c for details)

### Build Instructions

CHTTP uses a minimal C build system (`cbuild.h`). No Makefiles or CMake needed!

```bash
git clone https://github.com/grant-wade/chttp.git
cd chttp
gcc -o cbuild build.c
./cbuild
```

This will:

- Download dependencies (e.g., zlib) into `vendor/`
- Build the server into `build/server` and place a copy in the project root

### Running the Server

```bash
./server [OPTIONS]
```

#### Example

```bash
./server -p 8080 -d ./www
```

## Command-Line Interface

CHTTP uses a modern, type-safe CLI system (`cli.h`). All options support both short and long forms, with automatic help and defaults.

| Option             | Description                        | Default   |
|--------------------|------------------------------------|-----------|
| `-p`, `--port`     | Port to listen on                  | `4221`    |
| `-d`, `--directory`| Directory to serve static files    | `/tmp`    |
| `-v`, `--verbose`  | Enable verbose logging             | false     |
| `-h`, `--help`     | Show help message                  |           |

Example:

```bash
./server --port 8080 --directory ./public --verbose
```

## Server API Overview

The server is built around a modular API that allows you to compose your own server with routes and layers.

### Creating and Running a Server

```c
//! main.c
#include "server.h"
#include "router.h"
#include "builtin.h"

void hello_handler(HttpRequest* req, HttpResponse* res) {
    res->status = HTTP_200;
    res->body = string_new("Hello, World!", req->tag);
}

bool my_layer_fn(HttpRequest* req, HttpResponse* res) {
    printf("my_layer_fn called during pre routing")
    return true;
}

int main(int argc, char* argv[]) {
    // Parse CLI options (see cli.h for details)
    int port = 8080;
    bool verbose = false;

    // optional CLI library
    CLI_BEGIN(opts, argc, argv)
        CLI_INT('p', "port", port, 4221, "Port to listen on")
        CLI_FLAG('v', "verbose", verbose, "Enable verbose logging")
    CLI_END(opts)

    // Initialize server
    HttpServer server;
    http_server_init(&server, "0.0.0.0", port, TAG(&server));

    // Add built-in middleware and routes (logging, static files, etc.)
    http_server_add_builtins(&server, verbose);

    // Optionally add your own routes or layers here
    // (router, route, http_method, handler, exact match)
    router_add_route(server.router, "/hello", HTTP_GET, hello_handler, true);
    // Add a custom layer (middleware)
    // (layers, stage, name, function, can_fail)
    layers_add(server.layer_ctx, LAYER_PRE_ROUTE, "custom", my_layer_fn, false);

    // Start the server
    http_server_start(&server);

    // Cleanup
    http_server_free(&server);
    pallocator_cleanup();
    return 0;
}
```

### Building and Running the Example
```bash
$ ./cbuild run
...
COMPILE    src/utils.c
COMPILE    src/builtin.c
COMPILE    src/server.c
LINK       build/libhttp.a
COMPILE    src/main.c
LINK       build/server
COMMAND    copy server exe to root
SUBCMD     Running 'run': ./server
Server started on 0.0.0.0:4221
Waiting for a client to connect...
Client connected
RECV: /hello
Routing to GET /hello
SENT: HTTP/1.1 200 OK
MEM: 1.75 KB
TIME: 107 microseconds
Connection: keep-alive header found or not present, keeping connection open
Client closed connection
```


### Adding Built-in Middleware and Routes

The function `http_server_add_builtins(&server, verbose)` registers a set of useful built-in layers and routes:

- **Logging**: Request/response logging (basic or verbose).
- **Content-Encoding**: Gzip compression for supported clients.
- **Content-Length**: Automatic content-length header.
- **Connection Management**: Handles keep-alive and connection close.
- **Memory Usage**: Optionally logs memory usage per request.
- **Static Files**: Serves files from the configured directory.
- **Example Routes**: `/`, `/echo`, `/user-agent`, `/files/*`.

You can add or remove builtins, or register your own at any time.

### Registering a Custom Route

```c
#include "router.h"

void hello_handler(HttpRequest* req, HttpResponse* res) {
    // Set response body, status, etc.
}

router_add_route(server.router, "/hello", HTTP_GET, hello_handler, true);
```

### Adding Custom Middleware (Layer)

```c
#include "layers.h"

bool log_layer(HttpRequest* req, HttpResponse* res) {
    printf("Request: %s %s\n", http_request_method_to_string(req->request_line.method), string_cstr(req->request_line.target));
    return true;
}

layers_add(server.layer_ctx, LAYER_PRE_ROUTE, "logger", log_layer, false);
```

### Full Example

See `src/main.c` for a complete example of server setup, CLI parsing, and middleware/route registration.

## Memory Management

CHTTP uses a custom allocator (`alloc.h`) that allows you to tag allocations and free them in groups. This helps with debugging and prevents memory leaks.

- Use `pmalloc`, `pcalloc`, `prealloc`, `pfree`, and `pfree_tag`.
- Inspect allocations with `palloc_print_state()` and `pinspect()`.
- All major objects (requests, responses, routers, layers, etc.) accept a `tag` parameter for allocation tracking.

## Extending the Server

- **Add new routes**: Use `router_add_route`.
- **Add new middleware**: Use `layers_add` with any lifecycle stage.
- **Add custom subcommands**: See `cbuild.h` for build-time subcommands.
- **Inspect memory**: Use the allocator's debug functions.

## Development & Contributing

- **Pull requests** are welcome!
- Please open issues for bugs or feature requests.

## License

BSD-3 Clause License. See `LICENSE` file for details.

## Acknowledgments

- [CodeCrafters](https://app.codecrafters.io/) for the HTTP server course inspiration.
- [zlib](https://zlib.net/) for compression support.
