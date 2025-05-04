#include "router.h"
#include "alloc.h"
#include "http.h"
#include <stdio.h>

ARRAY_DEFINE(Route, RouteArray)

Router* router_new(void* tag) {
    Router* router = pmalloc(sizeof(Router), tag);
    if (!router) return NULL;

    router->routes.tag = tag;

    if (!RouteArray_init(&router->routes, tag)) {
        pfree(router);
        return NULL;
    }

    return router;
}

void router_free(Router* router) {
    if (!router) return;

    for (size_t i = 0; i < router->routes.size; i++) {
        Route* route = &router->routes.data[i];
        string_free(route->path);
    }

    RouteArray_destroy(&router->routes);
    pfree(router);
}

bool router_add_route(Router* router, const char* path, Methods methods, RouteHandler handler, bool exact_only) {
    if (!router || !path || !handler) return false;

    Route route;
    route.path = string_new(path, router->routes.tag);
    route.methods = methods;
    route.handler = handler;
    route.exact_only = exact_only;

    if (!RouteArray_push(&router->routes, route)) {
        string_free(route.path);
        return false;
    }

    return true;
}

bool router_route(Router* router, HttpRequest* request, HttpResponse* response) {
    if (!router || !request || !response) return false;

    for (size_t i = 0; i < router->routes.size; i++) {
        Route* route = &router->routes.data[i];
        if (!IN_METHODS(route->methods, request->request_line.method)) continue;
        if (route->exact_only && string_equals(route->path, request->request_line.target)) {
            printf("Routing to %s %s\n", http_request_method_to_string(request->request_line.method), route->path->data);
            route->handler(request, response);
            return true;
        } else if (!route->exact_only && string_begins_with(request->request_line.target, route->path)) {
            printf("Routing to %s %s\n", http_request_method_to_string(request->request_line.method), route->path->data);
            route->handler(request, response);
            return true;
        }
    }
    printf("No matching route found for %s %s\n", http_request_method_to_string(request->request_line.method), request->request_line.target->data);
    // set the response to 404
    response->status = "HTTP/1.1 404 Not Found";
    return false;
}

void router_print(Router* router) {
    if (!router) return;

    printf("Registered Routes:\n");
    for (size_t i = 0; i < router->routes.size; i++) {
        Route* route = &router->routes.data[i];
        printf("Path: %s, Method: %s\n", route->path->data, http_request_method_to_string(route->methods));
    }
}
