#include <stdio.h>
#include "alloc.h"
#include "cstring.h"
#include "http.h"
#include "router.h"
#include "routes.h"


String* file_search_dir;

void set_file_search_dir(String* dir) {
    if (file_search_dir) {
        string_free(file_search_dir);
    }
    file_search_dir = string_new(dir->data, dir->tag);
}


// this just returns 200 OK
void index_route(HttpRequest* request, HttpResponse* response) {
    if (!request || !response) {
        printf("Invalid request or response\n");
        return;
    }

    // Set the status line
    response->status = HTTP_200;
}

void echo_route(HttpRequest* request, HttpResponse*  response) {
    // this should include the headers Content-Type: text/plain and Content-Length
    // the body of the response should be the /echo/<message> part of the request
    if (!request || !response) {
        printf("Invalid request or response\n");
        return;
    }

    // Set the status line
    response->status = HTTP_200;

    // Set the content type header
    Header content_type_header = { .key = string_new("Content-Type", request->tag),
                                    .value = string_new("text/plain", request->tag) };

    if (!HeaderArray_push(response->headers, content_type_header)) {
        printf("Failed to add Content-Type header\n");
        return;
    }

    size_t content_length = string_length(request->request_line.target) - 6; // subtract length of "/echo/"
    String* body = string_substring(request->request_line.target, 6, content_length, request->tag);
    if (!body) {
        printf("Failed to create response body\n");
        return;
    }
    response->body = body;
}


// this should return the User-Agent header
void user_agent_route(HttpRequest* request, HttpResponse* response) {
    if (!request || !response) {
        printf("Invalid request or response\n");
        return;
    }

    // Set the status line
    response->status = HTTP_200;

    // Find the User-Agent header
    for (size_t i = 0; i < HeaderArray_size(request->headers); i++) {
        Header* header = HeaderArray_get_ptr(request->headers, i);
        if (string_equals_cstr(header->key, "User-Agent")) {
            // Set the body of the response to the User-Agent value
            response->body = string_substring(header->value, 0, string_length(header->value), request->tag);
            break;
        }
    }

    // Set the content type header
    Header content_type_header = { .key = string_new("Content-Type", request->tag),
                                    .value = string_new("text/plain", request->tag) };

    if (!HeaderArray_push(response->headers, content_type_header)) {
        printf("Failed to add Content-Type header\n");
        // set the response to 500
        response->status = HTTP_500;
        response->body = string_new("Internal Server Error", request->tag);
        return;
    }
}

void files_route(HttpRequest *request, HttpResponse *response) {
    // check if the request and response are valid
    if (!request || !response) {
        printf("Invalid request or response\n");
        return;
    }

    // get the filename from the request path
    String *filename = string_substring(
        request->request_line.target, 7,
        string_length(request->request_line.target) - 7,
        request->tag
    );

    // check if the filename is valid
    if (!filename || string_length(filename) == 0) {
        response->status = HTTP_400;
        response->body = string_new("Bad Request", request->tag);
        return;
    }

    // create a new string with the file search directory and the filename
    String *full_path = string_copy(file_search_dir, request->tag);
    string_append_cstr(full_path, "/");
    string_append(full_path, filename);

    if (request->request_line.method == HTTP_GET) {
        response->body = string_from_file(string_cstr(full_path), request->tag);
        if (!response->body) {
            printf("Failed to read file\n");
            response->status = HTTP_404;
            response->body = string_new("File Not Found", request->tag);
            return;
        }
        // set the content type header
        Header content_type_header = { .key = string_new("Content-Type", request->tag),
                                        .value = string_new("application/octet-stream", request->tag) };

        if (!HeaderArray_push(response->headers, content_type_header)) {
            printf("Failed to add Content-Type header\n");
            string_free(filename);
            response->status = HTTP_500;
            response->body = string_new("Internal Server Error", request->tag);
            return;
        }
        response->status = HTTP_200;
    } else if (request->request_line.method == HTTP_POST) {
        if (!string_to_file(request->body, string_cstr(full_path))) {
            printf("Failed to write file\n");
            response->status = HTTP_500;
            response->body = string_new("Internal Server Error", request->tag);
            return;
        }
        response->status = HTTP_201;
    }

}
