
#ifndef HTTP_BUILTIN_H
#define HTTP_BUILTIN_H

#include "http.h"

bool logging_layer_preroute_verbose(HttpRequest* request, HttpResponse* response);
bool logging_layer_postroute_verbose(HttpRequest* request, HttpResponse* response);
bool logging_layer_preroute_basic(HttpRequest* request, HttpResponse* response);
bool logging_layer_postroute_basic(HttpRequest* request, HttpResponse* response);
bool content_encoding_layer(HttpRequest* request, HttpResponse* response);
bool content_length_layer(HttpRequest* request, HttpResponse* response);
bool connection_close_layer(HttpRequest* request, HttpResponse* response);
bool request_memory_usage_layer(HttpRequest* request, HttpResponse* response);

#endif
