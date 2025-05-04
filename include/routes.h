#ifndef ROUTES_H
#define ROUTES_H

#include "http.h"

void set_file_search_dir(String* dir);

void index_route(HttpRequest *request, HttpResponse *response);
void echo_route(HttpRequest *request, HttpResponse *response);
void user_agent_route(HttpRequest *request, HttpResponse *response);
void files_route(HttpRequest *request, HttpResponse *response);

#endif
