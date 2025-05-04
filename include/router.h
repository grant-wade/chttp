
#ifndef ROUTER_H
#define ROUTER_H

#include "http.h"
#include "cstring.h"
#include "array.h"
#include <stdbool.h>

#define HTTP_200 "HTTP/1.1 200 OK";
#define HTTP_201 "HTTP/1.1 201 Created";
#define HTTP_400 "HTTP/1.1 400 Bad Request";
#define HTTP_404 "HTTP/1.1 404 Not Found";
#define HTTP_500 "HTTP/1.1 500 Internal Server Error";

// define the handler function type
typedef void (*RouteHandler)(HttpRequest* request, HttpResponse* response);

typedef struct {
    String* path;
    Methods methods;
    RouteHandler handler;
    bool exact_only;
} Route;

ARRAY_DECLARE(Route, RouteArray)

typedef struct {
    RouteArray routes;
} Router;

/**
 * @brief Creates a new router.
 * @param tag A memory allocation tag.
 * @return A pointer to the newly created router, or NULL on allocation failure.
 */
Router* router_new(void* tag);

/**
 * @brief Frees the memory allocated for the router and its routes.
 * @param router The router to free. If NULL, the function does nothing.
 */
void router_free(Router* router);


/**
 * @brief Adds a new route to the router.
 * @param router The router to add the route to.
 * @param path The path for the route (must be a valid UTF-8 string).
 * @param method The HTTP method for the route.
 * @param handler The function to handle requests for this route.
 * @param exact_only If `true`, the route will only match if the request path exactly matches the route path.
 * @return `true` if the route was added successfully, `false` otherwise (e.g., invalid parameters, allocation failure).
 */
bool router_add_route(Router* router, const char* path, Methods methods, RouteHandler handler, bool exact_only);

/**
 * @brief Finds a route that matches the request method and path.
 * @param router The router to search in.
 * @param request The HTTP request to match against.
 * @return A pointer to the matching route, or NULL if no match is found.
 */
bool router_route(Router* router, HttpRequest* request, HttpResponse* response);

/**
 * @brief Prints the routes in the router for debugging purposes.
 * @param router The router to print.
 */
void router_print(Router* router);

#endif
